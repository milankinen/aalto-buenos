
#include "tests/str.h"

#define MIN(arg1,arg2) ((arg1) > (arg2) ? (arg2) : (arg1))
#define MAX(arg1,arg2) ((arg1) > (arg2) ? (arg1) : (arg2))


static int vxnprintf(char*, int, const char*, va_list, int);

/* (almost) snprintf(3) */
int snprintf(char *str, int size, const  char  *fmt, ...) {
    va_list ap;
    int written;

    va_start(ap, fmt);
    written = vxnprintf(str, size, fmt, ap, 0);
    va_end(ap);
    return written;
}


/**
 * Compares two strings. The strings must be null-terminated. If the
 * strings are equal returns 0. If the first string is greater than
 * the second one, returns a positive value. If the first string is
 * less than the second one, returns a negative value. This function
 * works like the strncpy function in the C library.
 *
 * @param str1 First string to compare.
 *
 * @param str2 The second string to compare.
 *
 * @return The difference of the first pair of bytes in str1 and str2
 * that differ. If the strings are equal returns 0.
 */
int stringcmp(const char *str1, const char *str2)
{
    while(1) {
        if (*str1 == '\0' && *str2 == '\0')
            return 0;
        if (*str1 == '\0' || *str2 == '\0' ||
            *str1 != *str2)
            return *str1-*str2;

        str1++;
        str2++;
    }

    /* Dummy return to keep gcc happy */
    return 0;
}

/**
 * Copies a string from source to target. The target buffer should be
 * at least buflen long. At most buflen-1 characters are copied. The
 * copied string will be null-terminated. If the source string is
 * shorter than buflen-1 characters, the end of the target string will
 * be padded with nulls. This function is almost like the strcpy
 * function in C library. The only difference occurs when the source
 * string is longer than the target buffer. The C library function
 * does not return a null-terminated string in this case while this
 * function does.
 *
 * @param buflen The length of the target buffer.
 *
 * @param target The target buffer of the copy operation.
 *
 * @param source The string to be copied.
 *
 */
char *stringcopy(char *target, const char *source, int buflen)
{
    int i;
    char *ret;

    ret = target;

    for(i = 0; i < buflen - 1; i++) {
        *target = *source;
        if (*source == '\0') {
            i++;
            while(i < buflen) {
                *target = '\0';
                i++;
            }
            return ret;
        }
        target++;
        source++;
    }
    *target = '\0';

    return ret;
}


/**
 * Copies memory buffer of size buflen from source to target. The
 * target buffer should be at least buflen long.
 *
 * @param buflen The number of bytes to be copied.
 *
 * @param target The target buffer of the copy operation.
 *
 * @param source The source buffer to be copied.
 *
 */
void memcopy(int buflen, void *target, const void *source)
{
    int i;
    char *t;
    const char *s;
    uint32_t *tgt;
    const uint32_t *src;

    tgt = (uint32_t *) target;
    src = (uint32_t *) source;

    if(((uint32_t)tgt % 4) != 0 || ((uint32_t)src % 4) != 0 ) {
    t = (char *)tgt;
    s = (const char *)src;

    for(i = 0; i < buflen; i++) {
        t[i] = s[i];
    }

    return;
    }

    for(i = 0; i < (buflen/4); i++) {
        *tgt = *src;
    tgt++;
    src++;
    }


    t = (char *)tgt;
    s = (const char *)src;

    for(i = 0; i < (buflen%4); i++) {
    t[i] = s[i];
    }

    return;
}


/**
 * Sets size bytes in target to value.
 *
 * @param target The target buffer of the set operation.
 *
 * @param value What the bytes should be set to.
 *
 * @param size How many bytes to set.
 *
 */
void memoryset(void *target, char value, int size)
{
    int i;
    char *tgt;

    tgt = (char *)target;
    for(i = 0; i < size; i++)
        tgt[i] = value;
}

/** Converts the initial portion of a string to an integer
 * (e.g. "-23av34" converts to -23 and "a123" to 0). Works just like
 * atoi() in the C library. Errors are ignored, so too long numbers
 * (e.g. 12345678901234567890) will give "weird" results because of an
 * overflow occurs.
 *
 * @param s The string to convert
 * @return The converted numeric value
 */
int atoi(const char *s)
{
    int sign = 1;
    int i = 0;
    int value = 0;

    /* skip leading whitespace*/
    while(s[i] != 0 &&
      (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n'))
    i++;

    /* check possible sign */
    switch(s[i]) {
    case 0:
    return 0; /* only whitespace in s */
    case '-':
    sign = -1;
    /* and fall through */
    case '+':
    i++;
    }

    /* read chars until a nondigit (or end of string) is met */
    while(s[i] >= '0' && s[i] <= '9') {
    value = 10*value + sign*(int)(s[i]-'0');
    i++;
    }

    return value;
}

int strlen(const char *str)
{
    int l=0;

    while(*(str++) != 0) l++;

    return l;
}




#define FLAG_SMALLS  0x01
#define FLAG_ALT     0x02
#define FLAG_ZEROPAD 0x04
#define FLAG_LEFT    0x08
#define FLAG_SPACE   0x10
#define FLAG_SIGN    0x20


/* output the given char either to the string or to the TTY */
static void printc(char *buf, char c, int flags) {
    flags = flags;
    *buf = c;
}


/* Output 'n' in base 'base' into buffer 'buf' or to TTY.  At least
 * 'prec' numbers are output, padding with zeros if needed, and at
 * least 'width' characters are output, padding with spaces on the
 * left if needed. 'flags' tells whether to use the buffer or TTY for
 * output and whether to use capital digits.
 */
static int print_uint(char *buf,
              int size,
              unsigned int n,
              unsigned int base,
              int flags,
              int prec,
              int width)
{
    static const char digits[32] = "0123456789ABCDEF0123456789abcdef";
    char rev[11]; /* space for 32-bit int in octal */
    int i = 0, written = 0;

    if (size <= 0) return 0;

    /* produce the number string in reverse order to the temp buffer 'rev' */
    do {
    if (flags & FLAG_SMALLS)
        rev[i] = digits[16 + n % base];
    else
        rev[i] = digits[n % base];
    i++;
    n /= base;
    } while (n != 0);

    /* limit precision and field with */
    prec = MIN(prec, 11);
    width = MIN(width, 11);

    /* zero pad until at least 'prec' digits written */
    while(i < prec) {
    rev[i] = '0';
    i++;
    }

    /* pad with spaces until at least 'width' chars written */
    while(i < width) {
    rev[i] = ' ';
    i++;
    }

    /* output the produced string in reverse order */
    i--;
    while(i >= 0 && written < size) {
    printc(buf++, rev[i], flags);
    written++;
    i--;
    }

    return written;
}


/* Scan a 10-base nonnegative integer from string 's'. The scanned
 * integer is returned, and '*next' is set to point to the string
 * immediately following the scanned integer.
 */
static int scan_int(const char *s, const char **next) {
    int value = 0;

    while(*s > '0' && *s < '9') {
    value = 10*value + (int)(*s - '0');
    s++;
    }

    if (next != NULL) *next = s;
    return value;
}


/* Output a formatted string 'fmt' with variable arguments 'ap' into
 * the string 'buf' or to TTY, depending on 'flags'. At most 'size'
 * characters are written (including the trailing '\0'). Returns the
 * number of characters actually written.
 */
static int vxnprintf(char *buf,
             int size,
             const char *fmt,
             va_list ap,
             int flags)
{
    int written = 0, w, moremods;
    int width, prec;
    char ch, *s;
    unsigned int uarg;
    int arg;

    if (size <= 0) return 0;

    while(written < size) {
    ch = *fmt++;
    if (ch == '\0') break;

    /* normal character => just output it */
    if (ch != '%') {
        printc(buf++, ch, flags);
        written++;
        continue;
    }

    /* to get here, ch == '%' */
    ch = *fmt++;
    if (ch == '\0') break;

    width = prec = -1;
    moremods = 1;

    /* read flags and modifiers (width+precision): */
    do {
        switch(ch) {
        case '#': /* alternative output */
        flags |= FLAG_ALT;
        break;

        case '0': /* zero padding */
        flags |= FLAG_ZEROPAD;
        break;

        case ' ': /* space in place of '-' */
        flags |= FLAG_SPACE;
        break;

        case '+': /* '+' in place of '-' */
        flags |= FLAG_SIGN;
        break;

        case '-': /* left align the field */
        flags |= FLAG_LEFT;
        break;

        case '.': /* value precision */
        prec = scan_int(fmt, &fmt);
        break;

        case '1': case '2': case '3': case '4': case '5':
        case '6': case '7': case '8': case '9': /* field width */
        width = scan_int(fmt-1, &fmt);
        break;

        default: /* no more modifiers to scan */
        moremods = 0;
        }

        if (moremods) ch = *fmt++;
    } while(moremods && ch != '\0');


    if (ch == '\0') break;

    /* read the type of the argument : */
    switch(ch) {
    case 'i': /* signed integer */
    case 'd':
        arg = va_arg(ap, int);

        if (arg < 0) { /* negative value, print '-' and negate */
        printc(buf++, '-', flags);
        written++;
        arg = -arg;
        } if (flags & FLAG_SIGN) { /* '+' in place of '-' */
        printc(buf++, '+', flags);
        written++;
        } else if (flags & FLAG_SPACE) { /* ' ' in place of '-' */
        printc(buf++, ' ', flags);
        written++;
        }

        w = print_uint(buf, size-written, arg, 10, flags, 0, 0);
        buf += w;
        written += w;
        break;

    case 'o': /* octal integer */
        if (prec < width && (flags & FLAG_ZEROPAD)) prec = width;
        uarg = va_arg(ap, unsigned int);
        w = print_uint(buf, size-written, uarg, 8, flags, prec, width);
        buf += w;
        written += w;
        break;

    case 'u': /* unsigned integer */
        if (prec < width && (flags & FLAG_ZEROPAD)) prec = width;
        uarg = va_arg(ap, unsigned int);
        w = print_uint(buf, size-written, uarg, 10, flags, prec, width);
        buf += w;
        written += w;
        break;

    case 'p': /* memory pointer */
        flags |= FLAG_ALT;
    case 'x': /* hexadecimal integer, noncapitals */
        flags |= FLAG_SMALLS;
    case 'X': /* hexadecimal integer, capitals */

        if (flags & FLAG_ALT) { /* alt form begins with '0x' */
        printc(buf++, '0', flags);
        written++;
        if (written < size) {
            printc(buf++, 'x', flags);
            written++;
        }
        width -= 2;
        }
        if (prec < width && (flags & FLAG_ZEROPAD)) prec = width;

        uarg = va_arg(ap, unsigned int);
        w = print_uint(buf, size-written, uarg, 16, flags, prec, width);
        buf += w;
        written += w;
        break;

    case 'c': /* character */
        arg = va_arg(ap, int);
        printc(buf++, (char)arg, flags);
        written++;
        break;

    case 's': /* string */
        s = va_arg(ap, char*);
        w = size;
        if (prec != -1 && written+prec < size) w = written+prec;
        while(written < w && *s != '\0') {
        printc(buf++, *s++, flags);
        written++;
        }
        break;

    default: /* unknown type, just output */
        printc(buf++, ch, flags);
        written++;
    }
    }
    /* the string was truncated */
    if (written == size) {
    buf--;
    written = -1;
    }
    printc(buf, '\0', flags); /* terminating zero */

    return written;
}
