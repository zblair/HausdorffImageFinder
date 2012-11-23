/*=================================================================
* This application uses the directed Hausdorff distance to find
* the location of an object from a small image in a larger image.
*=================================================================*/
#define NOMINMAX
#pragma warning(disable : 4996) // Don't warn about using sprintf
#pragma warning(disable : 4530) // Don't warn about not enabling exceptions

#include <stdio.h>
#include <time.h>

// Image processing stuff
#include "image.h"
#include "hausdorff.h"

const char MATCH_PREVIEW_WINDOW_TITLE[] = "Match Preview";

// The image to search for in the haystack image
Image<Rgb>* needleImage;
Image<Intensity>* needleEdges;
Image<Intensity32F>* needleDistanceTransform;

// The image to search for the needle image in
Image<Rgb>* haystackImage;
Image<Intensity>* haystackEdges;
Image<Intensity32F>* haystackDistanceTransform;

// Used to display a preview of the needle edge image overlaid on the haystack
// edge image along with the computed Hausdorff distance between the two.
Image<Rgb>* matchPreviewImage;
CvFont* font;

/*
 * Finds the translation of needle in haystack that results in the minimal Hausdorff distance.
 */
CvPoint findBestTranslation(int step = 2, double* dist = 0,
                            int minX = 0, int minY = 0,
                            int maxX = -1, int maxY = -1)
{
    // Find the optimum translation
    unsigned bestX = 0;
    unsigned bestY = 0;
    double bestDistance = std::numeric_limits<double>::max();

    const int maxOffsetX = maxX != -1 ? maxX : haystackDistanceTransform->width() - needleEdges->width();
    const int maxOffsetY = maxY != -1 ? maxY : haystackDistanceTransform->height() - needleEdges->height();
    for (int y = minY; y < maxOffsetY; y += step)
    {
        for (int x = minX; x < maxOffsetX; x += step)
        {
            const double forwardDist = findHausdorffDistance(*needleEdges, *haystackDistanceTransform, cvPoint(x, y));
			const double reverseDist = findHausdorffDistance(*haystackEdges, *needleDistanceTransform, cvPoint(-x, -y));
			const double dist = std::max<double>(forwardDist, reverseDist);

            if (dist < bestDistance)
            {
                bestDistance = dist;
                bestX = x;
                bestY = y;
            }
        }
    }

    if (dist)
        *dist = bestDistance;

    return cvPoint(bestX, bestY);
}

/*
 * Finds the translation of needle in haystack that results in the minimal Hausdorff distance
 * by recurively calling findBestTranslation() for successively finer step sizes as we get
 * closer and closer to a solution.
 */
CvPoint findBestTranslationRecursive(int initialStep = 32, double* dist = 0)
{
    double bestDistance = std::numeric_limits<double>::max();
    CvPoint bestTranslation;

    int minX = 0;
    int minY = 0;
    const int absoluteMaxX = haystackDistanceTransform->width() - needleEdges->width();
    const int absoluteMaxY = haystackDistanceTransform->height() - needleEdges->height();
    int maxX = absoluteMaxX;
    int maxY = absoluteMaxY;

    for (int step = initialStep; step > 0; step /= 2)
    {
        double distance;
        CvPoint translation = findBestTranslation(
            step, &distance,
            minX, minY,
            maxX, maxY);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestTranslation = translation;

            minX = std::max<int>(0, translation.x - step);
            minY = std::max<int>(0, translation.y - step);
            maxX = std::min<int>(absoluteMaxX, translation.x + step);
            maxY = std::min<int>(absoluteMaxY, translation.y + step);
        }
    }

    if (dist)
        *dist = bestDistance;

    return bestTranslation;
}

/*
 * Finds the translation of needle in haystack that results in the minimal Hausdorff distance,
 * allowing for some variation in scale and rotation of the needle in the haystack image.
 */
double findBestTranslationScaleAndRotation(
                                  CvPoint* bestTranslation,
                                  int* bestRotation,
                                  double* bestScale,
                                  int initialTranslationStep = 32,
                                  int minRotation = -32,
                                  int maxRotation = 32,
                                  int rotationStep = 4,
                                  double minScale = 0.5,
                                  double maxScale = 2.0,
                                  double scaleStep = 0.25)
{
    double bestDistance = std::numeric_limits<double>::max();

    CvPoint2D32f center = cvPoint2D32f(needleEdges->width() / 2, needleEdges->height() / 2);
    CvMat* rotMat = cvCreateMat(2, 3, CV_32FC1);
    CvMat* bestRotMat = cvCreateMat(2, 3, CV_32FC1);
    Image<Intensity> backupPrior(needleEdges->width(), needleEdges->height());
    cvCopy(*needleEdges, backupPrior);

    for (int rotation = minRotation; rotation <= maxRotation; rotation += rotationStep)
    {
        double angle = rotation * 3.14 / 180.0;
        for (double scale = minScale; scale <= maxScale; scale += scaleStep)
        {
            cv2DRotationMatrix(center, angle, scale, rotMat);
            cvWarpAffine(backupPrior, *needleEdges, rotMat, 1+8, cvScalarAll(255));

            double dist;
            CvPoint point = findBestTranslationRecursive(initialTranslationStep, &dist);

            if (dist < bestDistance)
            {
                bestDistance = dist;
                if (bestTranslation)
					*bestTranslation = point;
				if (bestRotation)
					*bestRotation = rotation;
				if (bestScale)
					*bestScale = scale;
                cvCopy(rotMat, bestRotMat);
            }
        }
    }

    cvWarpAffine(backupPrior, *needleEdges, bestRotMat, 1+8, cvScalarAll(255));
    return bestDistance;
}

/*
 * Draws the translated needle shape in the haystack image.
 */
void drawTranslatedPrior(const CvPoint& point, double dist)
{
    // Superimpose the translated needle image on the haystack image
    cvCopy(*haystackImage, *matchPreviewImage);
	cvSetImageROI(*matchPreviewImage, cvRect(point.x, point.y, needleImage->width(), needleImage->height()));
	cvCopy(*needleImage, *matchPreviewImage);
	cvResetImageROI(*matchPreviewImage);
	cvCircle(*matchPreviewImage,
		cvPoint(point.x + needleImage->width()/2 - 10, point.y + needleImage->height()/2 - 10),
		20, cvScalar(0, 0, 255), 3);

    // Print the haystackDistanceTransform in the top-left corner of the image
    char buffer[64];
    sprintf(buffer, "dist = %.2f", dist);
    cvRectangle(*matchPreviewImage, cvPoint(0, 0), cvPoint(200, 30), cvScalar(255,255,255), CV_FILLED);
    cvPutText(*matchPreviewImage, buffer, cvPoint(10,20), font, cvScalar(0,0,0));

    cvShowImage(MATCH_PREVIEW_WINDOW_TITLE, *matchPreviewImage);
}

/*
 * Computes the Hausdorff distance if the needle were moved to the location of the mouse pointer
 * and displays the needle at that location along with the computed distance.
 */
void onMouseEvent(int /*evt*/, int x, int y, int flags, void* /*param*/)
{
    if (flags & CV_EVENT_FLAG_LBUTTON)
    {
        const int maxOffsetX = haystackDistanceTransform->width() - needleEdges->width();
        const int maxOffsetY = haystackDistanceTransform->height() - needleEdges->height();
        
        if (x <= maxOffsetX && y <= maxOffsetY)
        {
            const double forwardDist = findHausdorffDistance(*needleEdges, *haystackDistanceTransform, cvPoint(x, y));
			const double reverseDist = findHausdorffDistance(*haystackEdges, *needleDistanceTransform, cvPoint(-x, -y));
			const double dist = std::max<double>(forwardDist, reverseDist);

            // Superimpose the translated needle image on the haystack image
            drawTranslatedPrior(cvPoint(x, y), dist);
        }
    }
}

int main(int argc, char* argv[])
{
	if (argc != 3)
    {
        std::cerr << "Usage " << argv[0] << " needle.bmp haystack.bmp" << std::endl;
        return 1;
    }
    const char* needleFilename = argv[1];
	const char* haystackFilename = argv[2];

    // Initialize a font for drawing text on the image
    font = new CvFont;
    cvInitFont(font, CV_FONT_HERSHEY_COMPLEX_SMALL, 1.0, 1.0, 0, 1);

    // Open the haystack image and find the edges in it
	std::cout << "Opening " << haystackFilename << std::endl;
	try
	{
		haystackImage = new Image<Rgb>(haystackFilename);
		Image<Intensity>* tempHaystackImage = new Image<Intensity>(*haystackImage);
		haystackEdges = new Image<Intensity>(tempHaystackImage->width(), tempHaystackImage->height());
		cvSmooth(*tempHaystackImage, *tempHaystackImage);
		cvCanny(*tempHaystackImage, *haystackEdges, 30, 90);
		cvNot(*haystackEdges, *haystackEdges);
	}
	catch (const std::runtime_error&)
	{
		std::cerr << "Could not open " << haystackFilename << "." << std::endl;
		exit(EXIT_FAILURE);
	}

    // Create the matchPreviewImage image and display it in a window
    matchPreviewImage = new Image<Rgb>(haystackEdges->width(), haystackEdges->height());
    cvCopy(*haystackImage, *matchPreviewImage);
	cvNamedWindow(MATCH_PREVIEW_WINDOW_TITLE, CV_WINDOW_AUTOSIZE);
    cvShowImage(MATCH_PREVIEW_WINDOW_TITLE, *matchPreviewImage);

    // Open the needle image (the image to search for in haystack)
	std::cout << "Opening " << needleFilename << std::endl;
	try
	{
		needleImage = new Image<Rgb>(needleFilename);
		Image<Intensity>* tempNeedleImage = new Image<Intensity>(*needleImage);
		needleEdges = new Image<Intensity>(tempNeedleImage->width(), tempNeedleImage->height());
		cvSmooth(*tempNeedleImage, *tempNeedleImage);
		cvCanny(*tempNeedleImage, *needleEdges, 30, 90);
		cvNot(*needleEdges, *needleEdges);
	}
	catch (const std::runtime_error&)
	{
		std::cerr << "Could not open " << needleFilename << "." << std::endl;
		exit(EXIT_FAILURE);
	}

    // Calculate the distance transform of the haystack image
    haystackDistanceTransform = new Image<Intensity32F>(haystackEdges->width(), haystackEdges->height());
    cvDistTransform(*haystackEdges, *haystackDistanceTransform, CV_DIST_L1, CV_DIST_MASK_PRECISE, 0);

    // Calculate the distance transform of the needle image
    needleDistanceTransform = new Image<Intensity32F>(needleEdges->width(), needleEdges->height());
    cvDistTransform(*needleEdges, *needleDistanceTransform, CV_DIST_L1, CV_DIST_MASK_PRECISE, 0);

    // Allow the user to drag the needle image around using their mouse
    onMouseEvent(0, 0, 0, CV_EVENT_FLAG_LBUTTON, 0);
    cvSetMouseCallback(MATCH_PREVIEW_WINDOW_TITLE, onMouseEvent);

    std::cout << "Press ESC to exit." << std::endl
	          << "Press 'f' to find the best translation." << std::endl;

    for (;;)
    {
        int ch = cvWaitKey(0);
        if (ch == 27)    // Escape
            break;
        else if (ch == 'f') // Find the best translation
        {
            printf("\tFinding best translation...");

            clock_t start = clock();
            CvPoint bestTranslation;
			int bestRotation = 0;
			double bestScale = 1.0;
            double dist = findBestTranslationScaleAndRotation(
                                  &bestTranslation,	// Best translation pointer
                                  &bestRotation,	// Best rotation pointer
                                  &bestScale,		// Best scale pointer
                                  4,	// Initial translation step (in pixels)
                                  0,	// Minimum rotation (in degrees)
                                  0,	// Maximum rotation (in degrees)
                                  1,	// Rotation step (in degrees)
                                  1.0,  // Minimum scale (0, 1]
                                  1.0,	// Maximum scale (0, 1]
                                  1.0	// Scale step
							);

            clock_t finish = clock();

            drawTranslatedPrior(bestTranslation, dist);
            printf(" found at (%d, %d).\n", bestTranslation.x, bestTranslation.y);
            printf("\tSearch took %.2f secs\n", static_cast<double>(finish - start)/CLOCKS_PER_SEC);
        }
    }

    delete needleEdges;
    delete haystackEdges;
    delete needleDistanceTransform;
    delete haystackDistanceTransform;
    delete matchPreviewImage;
    delete font;

    return 0;
}