# Morse Code Character Device

This document outlines the specifications for a character device that allows
users to convert messages from plaintext to Morse code and vice versa. The
device exposes an interface for reading and writing data, allowing userspace
programs to interact with it.

The character device will be located at '/dev/morse'

## Operations:

### Open
    - Initializes internal structure required for the programmer.
    - Returns 0 on success, or an appropriate negative error code otherwise.


### Read

    - Reads from message device buffer in the Morse Code Converter device file and
    writes either the message encrypted in Morse code, or the message in
    plaintext (depending on ioctl) to the provided buffer.

    - On success, returns the number of characters written to the userpace buffer.
    - On failure, returns an appropriate negative error code.


### Write

    - Converts either plaintext messages into Morse code or vice versa.
    - Converts and stores both messages to separate buffers in the device,
    overwriting the previous buffer contents.

    - Arugments are a pointer to a userspace buffer and a size_t representing
    the length of the buffer in bytes.

    - Writes length bytes from the buffer to the device's plaintext buffer.
    - Converts the message from the userspace buffer to morse code and stores
    the converted message in the device's morse buffer.

    - The device's buffers are initially allocated 8 bytes, but will resize to
    fit the message if necesarry.

    - Returns the number of bytes written to the plaintext buffer, 
    or a negative error code on failure.


### Seek

    - Modifies the file position to a specific location within the device file
    according to the values of arguments 'offset' and 'whence'.

    - Seek is only available to the user when the device is in plaintext output
    mode.

    - Returns an appropriate negative error code when output mode is morse.
    - Returns (-EINVAL) if the resulting file position would be out of bounds.
    - Returns the new file offset on success.

    - The Morse Code Converter device file supports the following whence values:
    -SEEK_SET: Set the current position to offset bytes from &(buffer[0]).
    -SEEK_CUR: Set the current position to the current position plus offset.
    -SEEK_END: Set the current position to the end of the message plus offset.


### Ioctl

    - Ioctl supports the following three commands:

    - IOCTL_MORSE_RESET: a macro defined as _IO(0x11, 0)
    Reset the device to its initial state, clearing any
    stored data and restoring default settings. This ioctl takes no argument.
    Returns 0 on success, or a negative error code on failure.

    - IOCTL_MORSE_SET_PLAIN: a macro defined as _IO(0x11, 1)
    Set the device to plaintext output mode. This will
    cause any subsequent read calls to return the plaintext input that was written
    to the device, rather than the Morse code equivalent. This ioctl takes no
    argument. Returns 0 on success, or a negative error code on failure.

    - IOCTL_MORSE_SET_MORSE: a macro defined as _IO(0x11, 2)
    Set the device to Morse code output mode. This will
    cause any subsequent read calls to return the Morse code equivalent of the
    plaintext input that was written to the device. This ioctl takes no argument.
    Returns 0 on success, or a negative error code on failure.

    - Returns -EINVAL if an unsupported command is specified.
