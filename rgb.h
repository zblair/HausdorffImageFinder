#ifndef RGB_H
#define RGB_H

struct Rgb
{
    Rgb(unsigned char red, unsigned char green, unsigned char blue)
        : r(red), g(green), b(blue)
    {}

    // Color channels (red, green, and blue)
    // Note: The order of these declarations actually does matter
    // and no other data members can be added to this class because
    // it must be binary-compatible to the B, G, R format used by opencv
    unsigned char b;
    unsigned char g;
    unsigned char r;
};

#endif