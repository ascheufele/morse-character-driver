// This file implements the morse code character driver as outlined in Spec.txt
// Alex Scheufele
// 4/23
#include <linux/err.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/ctype.h>
#include <linux/string.h>
#include <linux/mutex.h>

#define IOCTL_MORSE_RESET _IO(0x11, 0)
#define IOCTL_MORSE_SET_PLAIN _IO(0x11, 1)
#define IOCTL_MORSE_SET_MORSE _IO(0x11, 2)
#define INITIAL_BUF_SIZE 8
#define MINOR_COUNT 1

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alex Scheufele");

ssize_t convert_to_morse(char *source, ssize_t n, char **target, ssize_t* target_length);

// internal structures
dev_t dev_num;
struct cdev *device;
struct class *morse_class;
struct device *morse_device;
struct morse
{
    ssize_t offset;
    struct mutex mtx;
    char *output_buf;
    ssize_t *output_buf_len;
    ssize_t morse_buf_len;      // length in bytes of the message currently stored in the buffer
    ssize_t plaintext_buf_len;  // length in bytes of the message currently stored in the buffer
    ssize_t plaintext_allocated_length; // bytes of memory allocated to plaintext buffer (used to resize)
    ssize_t morse_allocated_length;     // bytes of memory allocated to morse buffer (used to resize)
    char* morse_buf;
    char* plaintext_buf;
};
struct morse *_morse = NULL;

char *morse_alpha_table[] = {".- ", "-... ", "-.-. ", "-.. ", ". ", "..-. ", "--. ",
                             ".... ", ".. ", ".--- ", "-.- ", ".-.. ", "-- ", "-. ",
                             "--- ", ".--. ", "--.- ", ".-. ", "... ", "- ", "..- ",
                             "...- ", ".-- ", "-..- ", "-.-- ", "--.. "};
char *morse_digit_table[] = {"----- ", ".---- ", "..--- ", "...-- ", "....- ",
                             "..... ", "-.... ", "--... ", "---.. ", "----. "};

// open
static int morse_open(struct inode *inode, struct file *filp)
{
    if (_morse == NULL) { // kzalloc failed in init
        return -EBADFD; // File descriptor in bad state
    }
    filp->private_data = _morse; // all open file decriptors will share buffers
    return 0;
}

// read
static ssize_t morse_read(struct file *filp, char __user *usrbuf, size_t size, loff_t *offset)
{
    struct morse *_morse_private = (struct morse *)filp->private_data;
    unsigned long ret = 0;
    // take the lock so user can't ioctl change modes mid-read
    if (0 != mutex_lock_interruptible(&(_morse_private->mtx))) {
        return -ERESTART; // "Interrupted system call should be restarted"
    }
    // copy contents of output_buf to userspace
    if (size < *(_morse_private->output_buf_len)) {
        // user requests less data than available, give them size bytes.
        ret = copy_to_user(usrbuf, _morse_private->output_buf + _morse_private->offset, size);
        mutex_unlock(&(_morse_private->mtx));
        return size - ret;
    }
    if (size >= *(_morse_private->output_buf_len)) {
        // give them the full buffer
        ret = copy_to_user(usrbuf, _morse_private->output_buf + _morse_private->offset, (unsigned long)_morse_private->output_buf_len);
        mutex_unlock(&(_morse_private->mtx));
        return *(_morse_private->output_buf_len) - ret; // # of bytes successfully written
    }
    mutex_unlock(&(_morse_private->mtx));
    return -EINVAL;
}

// write
static ssize_t morse_write(struct file *filp, const char __user *usrbuf, size_t size, loff_t *offset)
{
    int ret = 0;
    ssize_t morse_ret = 0;
    // todo make sure there's enough space to write
    // todo make sure usrbuf is only morse compatible characters
    struct morse *_morse_private = (struct morse *)filp->private_data;
    // take the lock so user can't ioctl change modes mid-write
    if (0 != mutex_lock_interruptible(&(_morse_private->mtx))) {
        return -ERESTART; // "Interrupted system call should be restarted"
    }
    //***reset buffers since we're replacing the contents***
    // set buffer sizes to zero
    _morse_private->plaintext_buf_len = 0;
    _morse_private->morse_buf_len = 0;
    _morse_private->offset = 0; // reset offset
    // make sure we have sufficient space in driver's plaintext buffer, resize if necesarry.
    if(_morse_private->plaintext_allocated_length < size) {
        // preserve output buffer, its going to be a dangling pointer if we resize the pointer it points to
        // we could just make it a double pointer that stores the address of the pointer
        if(NULL == (_morse_private->plaintext_buf = krealloc(_morse_private->plaintext_buf, size, GFP_KERNEL)))
            return -ENOMEM;  // resize failed
        _morse_private->plaintext_allocated_length = size; // resize succeeded, update allocation length
    }
    // copy plaintext string from usrbuf to plaintext_buf
    ret = copy_from_user(_morse_private->plaintext_buf, usrbuf, size);
    if (0 != ret) { // copy_from_user failure
        return -ENOMEM;
    }
    _morse_private->plaintext_buf_len = size;
    // convert plaintext string to morse and store in morse buffer
    // if necesarry, the driver's morse buffer will be resized in covert_to_morse()
    morse_ret = convert_to_morse(_morse_private->plaintext_buf, _morse_private->plaintext_buf_len, 
        &(_morse_private->morse_buf), &(_morse_private->morse_allocated_length));
    _morse_private->morse_buf_len = morse_ret;
    mutex_unlock(&(_morse_private->mtx)); // free the lock
    return (size_t)size - ret;            // # of bytes successfully written to plaintext buffer
}

// ioctl
long morse_ioctl(struct file *filp, unsigned int command, unsigned long arg)
{
    struct morse *_morse_private = (struct morse *)filp->private_data;
    if (command == IOCTL_MORSE_RESET) {
        // set output buffer to plaintext
        _morse_private->output_buf = _morse_private->plaintext_buf;
        _morse_private->output_buf_len = &(_morse_private->plaintext_buf_len);
        // set buffer sizes to zero
        _morse_private->plaintext_buf_len = 0;
        _morse_private->morse_buf_len = 0;
        _morse_private->offset = 0; // reset offset
        return 0;
    }
    // switch private_data's pointers to modify output type
    if (command == IOCTL_MORSE_SET_MORSE) {
        _morse_private->output_buf = _morse_private->morse_buf;
        _morse_private->output_buf_len = &(_morse_private->morse_buf_len);
        return 0;
    }
    // switch private_data's pointers to modify output type
    if (command == IOCTL_MORSE_SET_PLAIN) {
        _morse_private->output_buf = _morse_private->plaintext_buf;
        _morse_private->output_buf_len = &(_morse_private->plaintext_buf_len);
        return 0;
    }
    return -EINVAL;
}

// seek
//  move the pointer for output_buf, perform bounds checking
//  return -EINVAL if OOB and offset otherwise
loff_t morse_llseek(struct file *filp, loff_t offset, int whence)
{
    loff_t ret;
    struct morse *_morse_private = (struct morse *)filp->private_data;
    // take the lock so user can't ioctl change modes mid-seek
    if (0 != mutex_lock_interruptible(&(_morse_private->mtx))) {
        return -ERESTART; // "Interrupted system call should be restarted"
    }
    if (_morse_private->output_buf == _morse_private->morse_buf) {
        mutex_unlock(&(_morse_private->mtx));
        goto seek_BAD_MODE; // can't seek in morse mode.
    }
    if (whence == SEEK_SET) {
        if (offset > *(_morse_private->output_buf_len))
            goto seek_OOB;
        _morse_private->offset = offset;
        ret = _morse_private->offset;
        _morse_private->output_buf_len -= offset;
        goto out;
    }
    if (whence == SEEK_CUR) {
        // if output_buf pointer plus proposed offset would be out of bounds of the message (ik its ugly)
        if (_morse_private->output_buf + _morse_private->offset + offset > _morse_private->output_buf + *(_morse_private->output_buf_len))
            goto seek_OOB;
        goto seek_success;
    }
    if (whence == SEEK_END) {
        // if output_buf pointer plus proposed offset would be out of bounds of the message (ik its ugly)
        if (_morse_private->output_buf + _morse_private->offset + offset > _morse_private->output_buf + *(_morse_private->output_buf_len))
            goto seek_OOB;
        goto seek_success;
    }
seek_BAD_MODE:
seek_OOB:
    ret = -EINVAL;
    goto out;
seek_success:
    ret = _morse_private->offset += offset;
    _morse_private->output_buf_len -= offset;
out:
    mutex_unlock(&(_morse_private->mtx));
    return (loff_t)ret;
}

// close
int morse_close(struct inode *inode, struct file *filp)
{
    return 0;
}

// define file operations
struct file_operations morse_fops = {
    .owner = THIS_MODULE,
    .read = morse_read,
    .write = morse_write,
    .llseek = morse_llseek,
    .unlocked_ioctl = morse_ioctl,
    .open = morse_open,
    .release = morse_close,
};

int __init morse_init(void)
{
    // alloc major and minor dev numbers dynamically
    if (0 != alloc_chrdev_region(&dev_num, 0, MINOR_COUNT, "morse")) {
        goto init_err;
    }
    device = cdev_alloc();
    device->owner = THIS_MODULE;
    cdev_init(device, &morse_fops);
    cdev_add(device, dev_num, 1);
    morse_class = class_create(THIS_MODULE, "morse");
    _morse = kzalloc(sizeof(struct morse), GFP_KERNEL);
    if (_morse == NULL) {
        goto init_err;
    }
    // set up data
    _morse->plaintext_allocated_length = INITIAL_BUF_SIZE;
    _morse->morse_allocated_length = INITIAL_BUF_SIZE;
    if(NULL == (_morse->plaintext_buf = kzalloc(sizeof(char) * INITIAL_BUF_SIZE, GFP_KERNEL)))
        goto init_err;
    if(NULL == (_morse->morse_buf = kzalloc(sizeof(char) * INITIAL_BUF_SIZE, GFP_KERNEL)))
        goto init_err;
    _morse->plaintext_buf_len = 0;
    _morse->output_buf_len = 0;
    _morse->output_buf = _morse->plaintext_buf; // default read mode is plaintext
    _morse->output_buf_len = &(_morse->plaintext_buf_len);
    _morse->offset = 0;
    mutex_init(&(_morse->mtx));
    morse_device = device_create(morse_class, NULL, dev_num, _morse, "morse");
    return 0;
init_err:
    kfree(device);
    class_destroy(morse_class);
    unregister_chrdev_region(dev_num, MINOR_COUNT);
    return -ENOMEM;
}

void __exit morse_exit(void)
{
    kfree(_morse->plaintext_buf);
    kfree(_morse->morse_buf);
    mutex_destroy(&(_morse->mtx));
    kfree(_morse);
    kfree(device);
    device_destroy(morse_class, dev_num);
    class_destroy(morse_class);
    unregister_chrdev_region(dev_num, MINOR_COUNT);
}

// stores the morse equivalent of the first n characters stored in the
// buffer located at the memory location pointed to by `source`
// at the memory location `target`.  The parameter target_length represents a
// pointer to an ssize_t storing the number of bytes currently allocated to
// target. This is necesarry because the function will attempt to resize the
// buffer if neccesary.The morse characters are separated by spaces. The final
// morse character is not followed by a space.
// The function returns the number of characters written to `target`.
ssize_t convert_to_morse(char *source, ssize_t n, char **target, ssize_t* target_length)
{
    int chars_written = 0;
    char* buf = *target;
    int offset = 'a';
    ssize_t i;
    char **table = NULL;
    char curr_char;
    for (i = 0; i < n; ++i)
    {
        // if alphabetical
        if (isalpha(source[i])) {
            table = morse_alpha_table;
            offset = 'a';
            curr_char = tolower(source[i]);
        }
        else if (isdigit(source[i])) {
            table = morse_digit_table;
            offset = '0';
            curr_char = source[i];
        }
        else {
            // invalid character, return number of bytes successfully written
            // before we encountered it.
            return chars_written - 1; // - 1 to omit the last space
        }
        // if we made it this far, we have a valid character
        for (int j = 0; j < strlen((table)[curr_char - offset]); ++j) {
            // write into target
            // check if we need to resize target
            if(chars_written + 1 > *target_length) {
                // attempt to resize (double the size)
                if(NULL == (buf = *target = krealloc(*target, *target_length * 2, GFP_KERNEL))) {
                    return chars_written; // resize failed
                }
                else {
                // update target_length by reference
                *target_length *= 2;
                // get buf back to where we left off with pointer arithmetic
                buf += chars_written;
                }
            }
            *buf = (table)[curr_char - offset][j];
            buf += 1;
            chars_written += 1;
        }
    }
    return chars_written - 1; // - 1 to omit the last space
}

module_init(morse_init);
module_exit(morse_exit);