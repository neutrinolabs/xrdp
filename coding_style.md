XRdp Coding Style
=================

The coding style used by XRdp is described below.

The XRdp project uses astyle (artistic style) to format the code. Astyle
requires a configuration file that describes how you want your code
formatted. This file is present in the XRdp root directory and is named
`astyle_config.as`.

Here is how we run the astyle command:

    astyle --options=/path/to/file/astyle_config.as "*.c"

This coding style is a work in progress and is still evolving.


Language Standard
-----------------

Try to make all code compile with both C and C++ compiler. C++ is more
strict, which makes the code safer.


Indentation
-----------

* 4 spaces per indent
* No tabs for any indents

☞

    if (fd < 0)
    {
        return -1;
    }


Line wrapping
-------------

* Keep lines not longer than 80 chars
* Align wrapped argument to the first argument

☞

    log_message("connection aborted: error %d",
                ret);


Variable names
--------------

* Use lowercase with underscores as needed
* Don't use camelCase
* Preprocessor constants should be uppercase

☞

    #define BUF_SIZE 1024
    int fd;
    int bytes_in_stream;


Variable declaration
--------------------

* Each variable is declared on a separate line

☞

    int i;
    int j;


Whitespace
----------

* Use blank lines to group statements
* Use at most one empty line between statements
* For pointers and references, use a single space before * or & but not after
* Use one space after a cast
* No space before square brackets

☞

    char *cptr;
    int *iptr;
    cptr = (char *) malloc(1024);

    write(fd, &buf[12], 16);


Function declarations
---------------------

* Use newline before function name

☞

    static int
    value_ok(int val)
    {
        return (val >= 0);
    }


Braces
------

* Opening brace is always on a separate line
* Align braces with the line preceding the opening brace

☞

    struct stream
    {
        int flags;
        char *data;
    };

    void
    process_data(struct stream *s)
    {
        if (stream == NULL)
        {
            return;
        }
    }


`if` statements
---------------

* Always use braces
* Put both braces on separate lines

☞

    if (val <= 0xff)
    {
        out_uint8(s, val);
    }
    else if (val <= 0xffff)
    {
        out_uint16_le(s, val);
    }
    else
    {
        out_uint32_le(s, val);
    }


`for` statements
----------------

* Always use braces
* Put both braces on separate lines

☞

    for (i = 0; i < 10; i++)
    {
        printf("i = %d\n", i);
    }


`while` and `do while` statements
---------------------------------

* Always use braces
* `while` after the closing brace is on the same line

☞

    while (cptr)
    {
        cptr—;
    }

    do
    {
        cptr--;
    } while (cptr);


`switch` statements
-------------------

* Indent `case` once
* Indent statements under `case` one more time
* Put both braces on separate lines

☞

    switch (cmd)
    {
        case READ:
            read(fd, buf, 1024);
            break;

        default:
            printf("bad cmd\n");
    }

Comments
--------

Use /* */ for comments
Don't use //
