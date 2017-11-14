#include <string.h>
#include <stdarg.h>

#include "strings.h"
#include "mm.h"

char *SDup(const char *str)
{
    char *r;
    size_t l;

    l = strlen(str);
    r = Mmalloc(l + 1);
    memcpy(r, str, l);
    r[l] = '\0';

    return r;
}

char *SConcat(const char *s1, const char *s2)
{
    return SMConcat(2, s1, s2);
}

char *SMConcat(size_t n, ...)
{
    char *r;
    const char **s;
    size_t *l;
    size_t i, c, d;
    va_list list;

    c = 0;
    va_start(list, n);
    s = (const char **)Mmalloc(sizeof(*s) * n);
    l = (size_t *)Mmalloc(sizeof(*l) * n);
    for (i = 0; i < n; i += 1)
    {
        s[i] = va_arg(list, const char *);
        l[i] = strlen(s[i]);
        c += l[i];
    }
    va_end(list);

    r = (char *)Mmalloc(c + 1);
    d = 0;
    for (i = 0; i < n; i += 1)
    {
        memcpy(r + d, s[i], l[i]);
        d += l[i];
    }
    r[c] = '\0';

    Mfree(s);
    Mfree(l);

    return r;
}
