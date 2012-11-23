/**
 * @file image.h Provides an image class to be used to load, manipulate, and store images.
 */

#ifndef IMAGE_H
#define IMAGE_H

// standard library
#include <string>
#include <exception>

// opencv
#include "cv.h"
#include "highgui.h"

// Pixel types
typedef unsigned char Intensity;
typedef float Intensity32F;
#include "rgb.h"

/**
* Represents an image.
* The Image class provides a simple interface for reading in and
* saving image files, as well as accessing individual pixel values
* and performing various algorithms on images.
*
* Two types are supported:
*   Image<Intensity> is an intensity image, where each pixel consists of
*       an 8-bit value.
*   Image<Rgb> is a colour image, where each pixel consists of three
*       8-bit values -- one for red, green, and blue.
*
* Much of the Image class' functionality is provided via opencv, and an object
* of the Image class can be used wherever a pointer to an OpenCv IplImage structure
* is required, because conversion to an IplImage pointer is automatic and computationally
* very cheap.
*/
template <typename T>
class Image {
public:
    Image(unsigned width, unsigned height);
    Image(std::string filename);

    // Conversions between colour and grayscale images
    Image(const Image<Rgb> & im);
    Image(const Image<Intensity> & im);
    Image(const Image<Intensity32F> & im);
    Image(IplImage * iplImg);

    Image<T>& operator= (const Image<T>& rhs);

    // Conversion to IplImage * used by OpenCV
    operator IplImage* ();

    // An image with no underlying image data is false
    operator bool() {
        return imageData.data() != 0;
    }

    // Image dimensions
    int width() const {
        return imageData.width();
    }
    int height() const {
        return imageData.height();
    }

    // Checked access to a pixel value by coordinate
    inline T & at(unsigned x, unsigned y);

    // Unchecked (faster) access to a pixel row by coordinate
    inline T* operator[](unsigned y);
    inline const T* operator[](unsigned y) const;

    // Writes the image to a file
    void save(std::string filename);

private:

    /// Underlying opencv image.
    CvImage imageData;
};

template <>
Image<Rgb>::Image(unsigned width, unsigned height)
    : imageData(cvSize(width, height), IPL_DEPTH_8U, 3)
{}

template <>
Image<Intensity>::Image(unsigned width, unsigned height)
    : imageData(cvSize(width, height), IPL_DEPTH_8U, 1)
{}

template <>
Image<Intensity32F>::Image(unsigned width, unsigned height)
    : imageData(cvSize(width, height), IPL_DEPTH_32F, 1)
{}

/**
* Constructs an Image<Intensity> image from the specified image file,
* converting the image to 8-bit grayscale if it is colour to begin with.
* Supports the following file types and extensions:
*   - Windows bitmaps - BMP, DIB
*   - JPEG files - JPEG, JPG, JPE
*   - Portable Network Graphics - PNG
*   - Portable image format - PBM, PGM, PPM
*   - Sun rasters - SR, RAS
*   - TIFF files - TIFF, TIF
*
* @param filename the filename of the file to read
*/
template <>
Image<Intensity>::Image(std::string filename)
    : imageData(filename.c_str(), 0, CV_LOAD_IMAGE_GRAYSCALE)
{
    // Check if the image has been loaded properly
    if( !imageData.data() ) {
        throw std::runtime_error("Failed to load image. Check that the path is correct.");
    }
}

/**
* Constructs an Image<Rgb> image from the specified image file.
*
* @param filename the filename of the file to read
*/
template <>
Image<Rgb>::Image(std::string filename)
    : imageData(filename.c_str(), 0, CV_LOAD_IMAGE_COLOR)
{
    // Check if the image has been loaded properly
    if( !imageData.data() ) {
        throw std::runtime_error("Failed to load image. Check that the path is correct.");
    }
}

/** Copy and conversion constructors */
template <>
Image<Intensity>::Image(const Image<Intensity> & im)
    : imageData(im.imageData) {}

template <>
Image<Intensity32F>::Image(const Image<Intensity32F> & im)
    : imageData(im.imageData) {}

template <>
Image<Rgb>::Image(const Image<Rgb> & im)
    : imageData(im.imageData) {}

template <>
Image<Intensity>::Image(const Image<Rgb> & im)
    : imageData(cvSize(im.width(), im.height()), IPL_DEPTH_8U, 1)
{
    Image<Rgb> * src = const_cast<Image<Rgb> *>(&im);
    cvCvtColor(*src, *this, CV_BGR2GRAY);
}

template <>
Image<Rgb>::Image(const Image<Intensity> & im)
    : imageData(cvSize(im.width(), im.height()), IPL_DEPTH_8U, 3)
{
    Image<Intensity> * src = const_cast<Image<Intensity> *>(&im);
    cvCvtColor(*src, *this, CV_GRAY2BGR);
}

template <typename T>
Image<T>::Image(IplImage * iplImg)
    : imageData(iplImg)
{}

template <typename T>
Image<T>& Image<T>::operator= (const Image<T>& rhs)
{
    if (this != &rhs) {

        // This should actually be a cheap operation, because
        // CvImage objects appear to keep the image data in a reference
        // counted structure, so a deep copy is not performed.
        imageData = rhs.imageData;
    }

    return *this;
}

/**
 * Conversion to IplImage so we can use Image objects with OpenCv
 * algorithms.
 *
 * Example
 * @code
 *   Image<Rgb> inputImg("lena.jpg");
 *   Image<Rgb> destImg(inputImg.width(), inputImg.height());
 *
 *   // Create a blurred image using OpenCV's "cvSmooth()" function
 *   cvSmooth( inputImg, destImg, CV_GAUSSIAN, 31, 31);
 * @endcode
 */
template <typename T>
Image<T>::operator IplImage* ()
{
    return static_cast<IplImage*>(imageData);
}

/**
* Access to a pixel value by coordinate.
* Because it returns a reference to the pixel value,
* this method can be used as an lvalue for changing
* pixel values, or as an rvalue for reading them.
*
* Example:
* @code
*   img.at(x, y) = Rgb(100, 20, 20);
*   Rgb colour = img.at(x, y);
* @endcode
*
* @param x the x coordinate of the pixel
* @param y the y coordinate of the pixel
* @return a reference to the pixel at (x, y)
*/
template <typename T>
inline T & Image<T>::at(unsigned x, unsigned y)
{
    if (0 <= x && x < width() &&
            0 <= y && y < height()) {
        return (*this)[y][x];
    }

    throw std::out_of_range("Image coordinates out-of-range");
}

/**
* Gets a pointer to a row by y-coordinate. Acquiring a row
* at a time and then accessing each pixel in the row is the
* fastest way to iterate through all the pixels in an image.
*
* Example:
* @code
*    for (unsigned y = 0; y < img.height(); y++) {
*        Rgb * row = img[y];
*        for (unsigned x = 0; x < img.width(); x++) {
*            Rgb pixel = row[x];
*            // Do something
*        }
*    }
* @endcode
*
* @param y the y coordinate of the row
* @return a pointer to a row at y
*/
template <typename T>
inline T* Image<T>::operator[](unsigned y)
{
    return reinterpret_cast<T*>(imageData.data() + y * imageData.step());
}
template <typename T>
inline const T* Image<T>::operator[](unsigned y) const
{
    return reinterpret_cast<const T*>(imageData.data() + y * imageData.step());
}

/**
* Writes the image to the specified file. Supports the following file
* types and extensions:
*   - Windows bitmaps - BMP, DIB
*   - JPEG files - JPEG, JPG, JPE
*   - Portable Network Graphics - PNG
*   - Portable image format - PBM, PGM, PPM
*   - Sun rasters - SR, RAS
*   - TIFF files - TIFF, TIF
*
* @param filename the filename of the file to create
*/
template <typename T>
void Image<T>::save(std::string filename)
{
    imageData.save(filename.c_str(), filename.c_str());
}

#endif

