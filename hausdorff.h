/**
 * @file hausdorff.h Provides algorithms for calculating the Hausdorff distance.
 */

#ifndef HAUSDORFF_H
#define HAUSDORFF_H

// Standard library
#include <limits>
#include <math.h>
#include <iostream>

// Opencv
#include "cv.h"

// Image wrapper
#include "image.h"

static const double MAX_HAUSDORFF_DISTANCE = 9999;

double findHausdorffDistance(
    const Image<Intensity>& imageA,
    const Image<Intensity32F>& imageB,
    const CvPoint& imageAOffset)
{
    const int width = imageA.width();
    const int height = imageA.height();
    double maxDistance = std::numeric_limits<double>::min();
    unsigned long pointsConsidered = 0;

    for (int iy = 0; iy < height; ++iy)
    {
        const int y = iy + imageAOffset.y;
        if (y < 0 || y > imageB.height())
            continue;

        const Intensity* const imageARow = imageA[iy];
        const Intensity32F* const imageBRow = imageB[y];
        for (int ix = 0; ix < width; ++ix)
        {
            // Only consider edge (black) pixels in the shape prior
            if (imageARow[ix] != 0)
                continue;

            const int x = ix + imageAOffset.x;
            if (x < 0 || x > imageB.width())
                continue;
            ++ pointsConsidered;
            double minDistance = imageBRow[x];
            if (minDistance > maxDistance)
            {
                maxDistance = minDistance;
            }
        }
    }

    return (pointsConsidered == 0) ? MAX_HAUSDORFF_DISTANCE : maxDistance;
}

#endif

