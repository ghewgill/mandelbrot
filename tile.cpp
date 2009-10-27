#include <stdio.h>
#include <unistd.h>
#include <complex>

#include <gd.h>
#include <gdfonts.h>

#include <fcgi_stdio.h>

#define RAINBOW
#define DEBUG
#ifdef DEBUG
    #undef DEBUG_BORDER
    #undef DEBUG_COORDS
    #undef DEBUG_ITERS
#endif

using namespace std;

void gd_putc(struct gdIOCtx *, int c)
{
    putchar(c);
}

int gd_putbuf(struct gdIOCtx *, const void *buf, int len)
{
    return fwrite((void *)buf, 1, len, stdout);
}

gdIOCtx fcgiout = {
    NULL,       //int (*getC) (struct gdIOCtx *);
    NULL,       //int (*getBuf) (struct gdIOCtx *, void *, int);
    gd_putc,    //void (*putC) (struct gdIOCtx *, int);
    gd_putbuf,  //int (*putBuf) (struct gdIOCtx *, const void *, int);
    NULL,       //int (*seek) (struct gdIOCtx *, const int);
    NULL,       //long (*tell) (struct gdIOCtx *);
    NULL,       //void (*gd_free) (struct gdIOCtx *);
};

class Generator {
public:
    virtual int iterate(const complex<long double> &p, int N) = 0;
};

class Mandelbrot: public Generator {
public:
    virtual int iterate(const complex<long double> &p, int N)
    {
        complex<long double> z = p;
        int n = 0;
        while (z.real()*z.real()+z.imag()*z.imag() < 4 && n < N) {
            z = z*z + p;
            n++;
        }
        return n;
    }
};

class Julia: public Generator {
public:
    Julia(const complex<long double> &c): c(c) {}
    virtual int iterate(const complex<long double> &p, int N)
    {
        complex<long double> z = p;
        int n = 0;
        while (z.real()*z.real()+z.imag()*z.imag() < 4 && n < N) {
            z = z*z + c;
            n++;
        }
        return n;
    }
private:
    const complex<long double> c;
};

void tile()
{
    long double x0 = -2;
    long double x1 = 1;
    long double y0 = -1.5;
    long double y1 = 1.5;
    int width = 100;
    int height = 100;
    long double tw = 0;
    char type = 'm';
    complex<long double> c;
    int iterations = 200;
    int ncolors = 200;

    //alarm(10);

    const char *query = getenv("QUERY_STRING");
    if (query != NULL) {
        char *q = strdup(query);
        char *p;
        while ((p = strsep(&q, "&")) != NULL) {
                 if (strncmp(p, "x0="    , 3) == 0) x0     = strtold(p+3, NULL);
            else if (strncmp(p, "x1="    , 3) == 0) x1     = strtold(p+3, NULL);
            else if (strncmp(p, "y0="    , 3) == 0) y0     = strtold(p+3, NULL);
            else if (strncmp(p, "y1="    , 3) == 0) y1     = strtold(p+3, NULL);
            else if (strncmp(p, "w="     , 2) == 0) width  = strtol(p+2, NULL, 10);
            else if (strncmp(p, "h="     , 2) == 0) height = strtol(p+2, NULL, 10);
            else if (strncmp(p, "tw="    , 3) == 0) tw     = strtold(p+3, NULL);
            else if (strncmp(p, "t="     , 2) == 0) type   = p[2];
            else if (strncmp(p, "c="     , 2) == 0) c      = complex<long double>(strtold(p+2, NULL), strchr(p, ',') ? strtold(strchr(p, ',')+1, NULL) : 0);
            else if (strncmp(p, "i="     , 2) == 0) iterations = strtol(p+2, NULL, 10);
        }
        free(q);
    }

    iterations = (int)(100 * log1p(tw > 0 ? 1/tw : 1/(x1-x0)));

    gdImagePtr img = gdImageCreate(width, height);
    int black = gdImageColorAllocate(img, 0, 0, 0);
    int white = gdImageColorAllocate(img, 255, 255, 255);
    int color[ncolors];
    for (int i = 0; i < ncolors; i++) {
#ifdef RAINBOW
        double c = 6.0*((i+0*ncolors/6)%ncolors) / ncolors;
        if (c < 1) {
            color[i] = gdImageColorAllocate(img,                0,   (int)(255*(c)),              255);
        } else if (c < 2) {
            color[i] = gdImageColorAllocate(img,                0,              255, (int)(255*(2-c)));
        } else if (c < 3) {
            color[i] = gdImageColorAllocate(img, (int)(255*(c-2)),              255,                0);
        } else if (c < 4) {
            color[i] = gdImageColorAllocate(img,              255, (int)(255*(4-c)),                0);
        } else if (c < 5) {
            color[i] = gdImageColorAllocate(img,              255,                0, (int)(255*(c-4)));
        } else {
            color[i] = gdImageColorAllocate(img, (int)(255*(6-c)),                0,              255);
        }
#else
        color[i] = gdImageColorAllocate(img, i*255/ncolors, i*255/ncolors, i*255/ncolors);
#endif
    }
    long double sx = (x1-x0)/width;
    long double sy = (y1-y0)/height;
    Generator *g;
    switch (type) {
        case 'm':
            g = new Mandelbrot();
            break;
        case 'j':
            g = new Julia(c);
            break;
        default:
            exit(1);
    }
    for (int ty = 0; ty < height; ty++) {
        long double y = y0 + ty*sy;
        int row[width];
        for (int tx = 0; tx < width; tx++) {
            long double x = x0 + tx*sx;
            row[tx] = g->iterate(complex<long double>(x, y), iterations);
        }
        for (int tx = 0; tx < width; tx++) {
            int n = row[tx];
            if (n < iterations) {
                gdImageSetPixel(img, tx, ty, color[n % ncolors]);
            } else {
                gdImageSetPixel(img, tx, ty, black);
            }
#ifdef DEBUG_BORDER
            if (tx == 0 || tx == width-1 || ty == 0 || ty == height-1) {
                gdImageSetPixel(img, tx, ty, white);
            }
#endif
        }
    }
#ifdef DEBUG_COORDS
    char buf[20];
    snprintf(buf, sizeof(buf), "%g,%g", x0, y0);
    gdImageString(img, gdFontSmall, 2, 2, (unsigned char *)buf, white);
#endif
#ifdef DEBUG_ITERS
    char buf[20];
    snprintf(buf, sizeof(buf), "iter=%d", iterations);
    gdImageString(img, gdFontSmall, 2, 2, (unsigned char *)buf, white);
#endif
    delete g;
    printf("Content-type: image/png\n");
    printf("Cache-control: public\n");
    printf("\n");
    gdImagePngCtx(img, &fcgiout);
    gdImageDestroy(img);
}

int main(int argc, char *argv[])
{
    while (FCGI_Accept() >= 0) {
        tile();
    }
    return 0;
}
