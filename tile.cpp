#include <stdio.h>
#include <unistd.h>
#include <complex>

#include <gd.h>
#include <gdfonts.h>

#include <fcgi_stdio.h>

#undef RAINBOW
#define DEBUG
#ifdef DEBUG
    #undef DEBUG_BORDER
    #define DEBUG_COORDS
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
    virtual int iterate(const complex<double> &p, int N) = 0;
};

class Mandelbrot: public Generator {
public:
    virtual int iterate(const complex<double> &p, int N)
    {
        complex<double> z = p;
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
    Julia(const complex<double> &c): c(c) {}
    virtual int iterate(const complex<double> &p, int N)
    {
        complex<double> z = p;
        int n = 0;
        while (z.real()*z.real()+z.imag()*z.imag() < 4 && n < N) {
            z = z*z + c;
            n++;
        }
        return n;
    }
private:
    const complex<double> c;
};

void tile()
{
    double x0 = -2;
    double x1 = 1;
    double y0 = -1.5;
    double y1 = 1.5;
    int width = 100;
    int height = 100;
    char type = 'm';
    complex<double> c;
    int iterations = 200;
    int ncolors = 200;

    //alarm(10);

    const char *query = getenv("QUERY_STRING");
    if (query != NULL) {
        char *q = strdup(query);
        char *p;
        while ((p = strsep(&q, "&")) != NULL) {
                 if (strncmp(p, "x0="    , 3) == 0) x0     = strtod(p+3, NULL);
            else if (strncmp(p, "x1="    , 3) == 0) x1     = strtod(p+3, NULL);
            else if (strncmp(p, "y0="    , 3) == 0) y0     = strtod(p+3, NULL);
            else if (strncmp(p, "y1="    , 3) == 0) y1     = strtod(p+3, NULL);
            else if (strncmp(p, "w="     , 2) == 0) width  = strtol(p+2, NULL, 10);
            else if (strncmp(p, "h="     , 2) == 0) height = strtol(p+2, NULL, 10);
            else if (strncmp(p, "t="     , 2) == 0) type   = p[2];
            else if (strncmp(p, "c="     , 2) == 0) c      = complex<double>(strtod(p+2, NULL), strchr(p, ',') ? strtod(strchr(p, ',')+1, NULL) : 0);
            else if (strncmp(p, "i="     , 2) == 0) iterations = strtol(p+2, NULL, 10);
        }
        free(q);
    }

    gdImagePtr img = gdImageCreate(width, height);
    int black = gdImageColorAllocate(img, 0, 0, 0);
    int white = gdImageColorAllocate(img, 255, 255, 255);
    int color[ncolors];
    for (int i = 0; i < ncolors; i++) {
#ifdef RAINBOW
        double c = 6.0*((i+0*ncolors/6)%ncolors) / ncolors;
        if (c < 1) {
            color[i] = gdImageColorAllocate(img, 255, (int)(255*(c)), 0);
        } else if (c < 2) {
            color[i] = gdImageColorAllocate(img, (int)(255*(2-c)), 255, 0);
        } else if (c < 3) {
            color[i] = gdImageColorAllocate(img, 0, 255, (int)(255*(c-2)));
        } else if (c < 4) {
            color[i] = gdImageColorAllocate(img, 0, (int)(255*(4-c)), 255);
        } else if (c < 5) {
            color[i] = gdImageColorAllocate(img, (int)(255*(c-4)), 0, 255);
        } else {
            color[i] = gdImageColorAllocate(img, 255, 0, (int)(255*(6-c)));
        }
#else
        color[i] = gdImageColorAllocate(img, 0, 0, i*255/ncolors);
#endif
    }
    color[ncolors-1] = white;
    double sx = (x1-x0)/width;
    double sy = (y1-y0)/height;
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
        double y = y0 + ty*sy;
        for (int tx = 0; tx < width; tx++) {
            double x = x0 + tx*sx;
            int n = g->iterate(complex<double>(x, y), iterations);
            if (n < iterations) {
                gdImageSetPixel(img, tx, ty, color[n*ncolors/iterations]);
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