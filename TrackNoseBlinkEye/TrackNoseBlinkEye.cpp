
/* 
* Control mouse using Nose and Eye 
* 
*/

#include <stdio.h>
#include <opencv2\opencv.hpp>
#include <opencv2\highgui\highgui.hpp>

// Nose tracking defines
#define NOSE_TPL_WIDTH      10
#define NOSE_TPL_HEIGHT     10

#define NOSE_WINDOW_WIDTH   18
#define NOSE_WINDOW_HEIGHT  18

#define BOUNDARY_WINDOW_WIDTH   75
#define BOUNDARY_WINDOW_HEIGHT  75

#define NOSE_THRESHOLD 0.4

// Nose track Functions Declaration
void mouseHandler( int event, int x, int y, int flags, void *param );
void trackObject();
void mouseMove(bool top, bool right, bool bottom, bool left);

// Eye Blink Functions Declaration
int  get_connected_components(IplImage* img, IplImage* prev, CvRect window, CvSeq** comp);
int  is_eye_pair(CvSeq* comp, int num, CvRect* eye);
int  locate_eye(IplImage* img, IplImage* eye_template, CvRect* window, CvRect* eye);
int  is_blink(CvSeq* comp, int num, CvRect window, CvRect eye);
void delay_frames(int nframes);
void exit_nicely(char* msg);
CvCapture   *capture;
CvFont font;
CvMemStorage*   storage;
#define STAGE_INIT      1
#define STAGE_TRACKING  2

IplConvKernel*  kernel;
#define POINT_TL(r)     cvPoint(r.x, r.y)
#define POINT_BR(r)     cvPoint(r.x + r.width, r.y + r.height)
#define POINTS(r)       POINT_TL(r), POINT_BR(r)
#define EYE_TPL_WIDTH       15
#define EYE_TPL_HEIGHT      15
#define EYE_WIN_WIDTH       EYE_TPL_WIDTH * 2
#define EYE_WIN_HEIGHT      EYE_TPL_HEIGHT * 2
#define EYE_THRESHOLD		0.6


// Define block for drawing rectangle on captured frame 
#define DRAW_RECTS(f, d, rw, ro)                                \
do {                                                            \
       \
    cvRectangle(f, POINTS(ro), CV_RGB(0, 0, 255), 1, 8, 0);     \
        \
    cvRectangle(d, POINTS(ro), cvScalarAll(255),  1, 8, 0);     \
} while(0)
 
// Nose or selected region tracking declarations
IplImage *frame, *eye_template, *template_match, *gray, *prev, *diff, *nose_template, *nose_template_match;
CvRect window_boundary,box;
POINT mouse_cursor;
int template_edge_x, template_edge_y, is_tracking = 0;
int startpos_x,startpos_y;
int boundry_x,boundry_y,search_window_x,search_window_y,move_left=0,move_up=0;
double  nose_min_value=0, nose_max_value=0;
CvPoint nose_min_location,nose_max_location;


int main( int argc, char** argv )
{
	CvSeq* comp = 0;
	CvRect window, eye;
	int key, nc, found;
	int stage = STAGE_INIT;
		
	key=0;
	startpos_x = 0;
	startpos_y = 0;
	search_window_x=0,search_window_y=0;		
		
	/* Initialize Capture from webcam
	*	Here '0' in cvCaptureCAM indicates the Index of the camera to be used.		
	*/
	capture = cvCaptureFromCAM(0);
	if (!capture)
		exit_nicely("Webcam Not found!");
 
	cvSetCaptureProperty(capture, CV_CAP_PROP_FRAME_WIDTH,  300);
	cvSetCaptureProperty(capture, CV_CAP_PROP_FRAME_HEIGHT, 250);
 
	// Grabs and returns a frame from the camera.
	frame = cvQueryFrame(capture);

	if (!frame)
		exit_nicely("Cannot query frame!");
 
	// Create a window named 'Video' with window size Normal.
	cvNamedWindow("video",CV_WINDOW_NORMAL);

	/*  * Creates Windows Handler for keeping the frame window
		* always on top of other windows.
	*/
	HWND win_handle = FindWindow(0, "video");
	if (!win_handle)
	{
		printf("Failed FindWindow\n");
	}
	
	SetWindowPos(win_handle, HWND_TOPMOST, 0, 0, 0, 0, 1);
	ShowWindow(win_handle, SW_SHOW);

	
	// Create a callback to mousehandler when mouse is clicked on frame.
	cvSetMouseCallback( "video", mouseHandler,NULL );

	// cvCreateImage is used to create & allocate image data
	nose_template = cvCreateImage( cvSize( NOSE_TPL_WIDTH, NOSE_TPL_HEIGHT ),frame->depth, frame->nChannels );
	nose_template_match = cvCreateImage( cvSize( BOUNDARY_WINDOW_WIDTH  - NOSE_TPL_WIDTH  + 1, BOUNDARY_WINDOW_HEIGHT - NOSE_TPL_HEIGHT + 1 ),IPL_DEPTH_32F, 1 );
	
	// Initialize Memory for storing the frames
	storage = cvCreateMemStorage(0);	
	if (!storage)
		exit_nicely("Cannot allocate memory storage!");
 
	/* Allocates and Fills the structure IplConvKernel ,
	which can be used as a structuring element in the morphological operations */
	kernel = cvCreateStructuringElementEx(3, 3, 1, 1, CV_SHAPE_CROSS, NULL);
	gray   = cvCreateImage(cvGetSize(frame), 8, 1);
	prev   = cvCreateImage(cvGetSize(frame), 8, 1);
	diff   = cvCreateImage(cvGetSize(frame), 8, 1);
	eye_template    = cvCreateImage(cvSize(EYE_TPL_WIDTH, EYE_TPL_HEIGHT), 8, 1);
 
	// Show if any error occurs during allocation
	if (!kernel || !gray || !prev || !diff || !eye_template)
		exit_nicely("System error.");
 
	gray->origin  = frame->origin;
	prev->origin  = frame->origin;
	diff->origin  = frame->origin;

	// Loop until ESC(27) key is pressed
	while( key != 27 ) 
	{
		// Get a frame from CAM
		frame = cvQueryFrame( capture );

		/* Always check if frame exists */
		if( !frame ) break;

		// Flip the frame vertically
		cvFlip( frame, frame, 1 );

		frame->origin = 0;

		// Eye blink detection & tracking
		if (stage == STAGE_INIT)
			window = cvRect(0, 0, frame->width, frame->height);

		// Convert original image to thresholded(grayscale) image for efficient detection*/
		cvCvtColor(frame, gray, CV_BGR2GRAY);

		// Find connected components in the image
		nc = get_connected_components(gray, prev, window, &comp);
			
		// Check if eyes are detected and start tracking by setting Region of Interest(ROI)
		if (stage == STAGE_INIT && is_eye_pair(comp, nc, &eye))
		{
			cvSetImageROI(gray, eye);
			cvCopy(gray, eye_template, NULL);
			cvResetImageROI(gray);
			// Start tracking eyes for blink
			stage = STAGE_TRACKING;				
		}
			
		// Here the tracking will start & will check for eye blink
		if (stage == STAGE_TRACKING)
		{
			// Try to locate the eye
			found = locate_eye(gray, eye_template, &window, &eye);
				
			// If eye is not found here or 'r' is pressed, restart the eye detection module
			if (!found || key == 'r')
				stage = STAGE_INIT;
				
			DRAW_RECTS(frame, diff, window, eye);
			// Check if there was an eye blink
			if (is_blink(comp, nc, window, eye))
			{
				//DRAW_RECTS(frame, diff, window, eye);
				printf("Eye Blinked!");

				// Perform mouse left button click on blink
				mouse_event(MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP, 0,0,0,0);
			}				
		}
		
		prev = (IplImage*)cvClone(gray);

		/* Perform nose tracking if template is available 
			Here tracking will start & continues till
			selected templated is within specified 
			threshold limit */
		if( is_tracking ) trackObject();

		// Display the frame in window
		cvShowImage( "video", frame );

		// Check for a key press
		key = cvWaitKey( 10 );
	}

	// Exit without any error
	exit_nicely(NULL);
}

/* Mouse handler is used for capturing users click event on the capture frame.
 * It detects a click & draws rectangle on the frame which is used for tracking 
 * the selected ROI by template matching */
void mouseHandler( int event, int x, int y, int flags, void *param )
{
        
	// User clicked on frame, save subimage as template
	if( event == CV_EVENT_LBUTTONUP )
	{
		template_edge_x = x - ( NOSE_TPL_WIDTH  / 2 );
		template_edge_y = y - ( NOSE_TPL_HEIGHT / 2 ); 

		cvSetImageROI( frame, 
                       cvRect( template_edge_x, 
                               template_edge_y, 
                               NOSE_TPL_WIDTH, 
                               NOSE_TPL_HEIGHT ) );
        cvCopy( frame, nose_template, NULL );

		// Reset previous ROI for selecting the updated ROI
        cvResetImageROI( frame );

        /* template is available, start tracking! 
			& set the specified points as starting 
			or reference point */
        fprintf( stdout, "Template selected. Start tracking... \n" );
        startpos_x = template_edge_x;
        startpos_y = template_edge_y;       

        // Set tracking flag
		is_tracking = 1;
    }
}

/*	* The actual tracking of object that is selected as ROI is being done from this block.
	* On ROI selection two rectangles are drawn one is red which is reference point of the 
	* selected object which will allow you to stop the mouse movement if moved in that area.
	* Another rectangle is green which will track your motion & will move mouse accordingly.		
*/
void trackObject()
{
        CvPoint minloc=cvPoint(0,0), maxloc=cvPoint(0,0);
		nose_min_value=0, nose_max_value=0;
		bool top = false,right = false,bottom = false,left = false;
		/* setup position of search window */
		search_window_x = startpos_x - ( ( NOSE_WINDOW_WIDTH  - NOSE_TPL_WIDTH  ) / 2 );
		search_window_y = startpos_y - ( ( NOSE_WINDOW_HEIGHT - NOSE_TPL_HEIGHT ) / 2 );

		/*  Setup boundry for tracking outside 
			this boundry tracking will not be done */
		boundry_x = startpos_x - ( ( BOUNDARY_WINDOW_WIDTH - NOSE_TPL_WIDTH) / 2 );
		boundry_y = startpos_y - ( ( BOUNDARY_WINDOW_HEIGHT - NOSE_TPL_HEIGHT) / 2 );
		
		// Move Mouse Pointer to UP
		if( template_edge_y  < search_window_y )
		{
			top = true;
		}

		// Move Mouse Pointer to Right
		if((template_edge_x + NOSE_TPL_WIDTH) > (search_window_x + NOSE_WINDOW_WIDTH))
		{
			right = true;
		}

		// Move Mouse Pointer to Down
		if(( template_edge_y + NOSE_TPL_HEIGHT) > (search_window_y + NOSE_WINDOW_HEIGHT))
		{
			bottom = true;
		}

		// Move Mouse Pointer to Left		
		if(template_edge_x < search_window_x)
		{
			left = true;
		}

		/*  * Call to mouseMove() method which will move
			* the mouse pointer according to your nose
			* movement.
		*/
		mouseMove(top,right,bottom,left);

		/*  setup search window & replace the 
			predefined EYE_WIN_WIDTH and EYE_WIN_HEIGHT above for your convenient */
		window_boundary = cvRect( boundry_x,
								  boundry_y, 
								  BOUNDARY_WINDOW_WIDTH, 
								  BOUNDARY_WINDOW_HEIGHT );

        /*	Make sure that the search window is still within the frame 
			This will make the bounding box to the search window
			within the frame. */
		if (window_boundary.x < 0)
	        window_boundary.x = 0;
		if (window_boundary.y < 0)
			window_boundary.y = 0;
		if (window_boundary.x + window_boundary.width > frame->width)
	        window_boundary.x = frame->width - window_boundary.width;
		if (window_boundary.y + window_boundary.height > frame->height)
	        window_boundary.y = frame->height - window_boundary.height;

		// Set Region of Interest
        cvSetImageROI(frame,window_boundary);

		/*  * Match the template with the stored template
			* The NORMED calculations give values upto 1.0… the other ones return huge values. 
			* SQDIFF is a difference based calculation that gives a 0 at a perfect match.			
		*/
		cvMatchTemplate( frame, nose_template, nose_template_match, CV_TM_SQDIFF_NORMED );

		/*  * To determine maximum point in correlation we use another opencv function 'cvMinMaxLoc()'
			* This function returns Minimum and Maximum values in the array (nose_template_match) and return their locations.
			* Now 'nose_min_value' holds the point we’re interested in: the point with maximum correlation.
		*/
		cvMinMaxLoc( nose_template_match, &nose_min_value, &nose_max_value, &nose_min_location, &nose_max_location, 0 );

		// Reset Region of Interest
		cvResetImageROI( frame );

		/* Check if the selected object is within specified 
		threshold limit & update the objects location
		& redraw the rectangle on updated location */
		if( nose_min_value <= NOSE_THRESHOLD ) 
		{
			/* save object's current location */
            template_edge_x = window_boundary.x + nose_min_location.x;
            template_edge_y = window_boundary.y + nose_min_location.y;

			/*  Rectangular area for selected template indicated 
				by red rectangle used to control your mouse movement.*/
            cvRectangle( frame,
					 cvPoint( template_edge_x , template_edge_y ),
                     cvPoint( template_edge_x + (NOSE_TPL_WIDTH), template_edge_y + (NOSE_TPL_HEIGHT)),
                     cvScalar( 0, 0, 255, 0 ), 1, 0, 0 );
			
			/*  Rectanglular area for search indicated by green 
				rectangle used to define boundary for your mouse movement.
				As your selected template indicated by red rectangle moves just outside
				green rectangle mouse moves accordingly in that direction.*/
			cvRectangle( frame,
				cvPoint( search_window_x, search_window_y ),
					 cvPoint( search_window_x + NOSE_WINDOW_WIDTH, search_window_y + NOSE_WINDOW_HEIGHT ),
                     cvScalar( 0, 255, 0, 0 ), 1, 0, 0 );

			/*	Rectangular area indicating the boundry by blue
				rectangle used to define the tracking area.
			*/
			cvRectangle( frame,
				cvPoint( window_boundary.x, window_boundary.y ),
				cvPoint( window_boundary.x + window_boundary.width, window_boundary.y + window_boundary.height ),
                    cvScalar( 255, 0, 0, 0 ), 1, 0, 0 );

		}
		/* If not within threshold limit */
		else 
		{			
			fprintf( stdout, "Lost object.\n" );		
			is_tracking = 0;
		}
}

/*  * mouseMove() will move the mouse pointer in the
	* direction of nose movement.
	* mouseMove takes four arguments & accordingly increment
	* or decrement the position of movement.
*/
void mouseMove(bool top, bool right, bool bottom, bool left)
{	
	if(top)
	{
		move_up--;
	}
	if(right)
	{
		move_left++;
	}
	if(bottom)
	{
		move_up++;
	}
	if(left)
	{
		move_left--;
	}
	if(top || right || bottom || left)
	{
		/* GetCursorPos reads the current cursor position on screen. */
		GetCursorPos(&mouse_cursor);

		/* SetCursorPos sets the cursor position according to movement. */
		SetCursorPos(mouse_cursor.x + move_left, mouse_cursor.y + move_up);
	}
	else
	{
		move_up = move_left = 0;
	}
}

/*	The function will return the connected components in 'comp', 
	as well as the number of connected components 'nc'.
	At this point, we have to determine whether the components are eye pair or not.
	We'll use experimentally derived heuristics for this, based on the width, 
	height, vertical distance, and horizontal distance of the components. 
	To make things simple, we only proceed if the number of the connected components is 2.*/
int get_connected_components(IplImage* img, IplImage* prev, CvRect window, CvSeq** comp)
{
		IplImage* _diff;
 
		cvZero(diff);
 
    /* apply search window to images */
		cvSetImageROI(img, window);
		cvSetImageROI(prev, window);
		cvSetImageROI(diff, window);
 
    /* motion analysis */
		cvSub(img, prev, diff, NULL);
		cvThreshold(diff, diff, 5, 255, CV_THRESH_BINARY);
		cvMorphologyEx(diff, diff, NULL, kernel, CV_MOP_OPEN, 1);
 
    /* reset search window */
		cvResetImageROI(img);
		cvResetImageROI(prev);
		cvResetImageROI(diff);
 
		_diff = (IplImage*)cvClone(diff);
 
    /* get connected components */
		int nc = cvFindContours(_diff, storage, comp, sizeof(CvContour),
                            CV_RETR_CCOMP, CV_CHAIN_APPROX_SIMPLE, cvPoint(0,0));
 
		cvClearMemStorage(storage);		
		cvReleaseImage(&_diff);
	
		return nc;
}

/*	Here are the rules applied to determine whether connected components are eye pair:
    1) The width of the components are about the same.
    2) The height of the components are about the same.
    3) Vertical distance is small.
    4) Reasonable horizontal distance, based on the components' width.
	If the components successfully pass the filter above, the system will continue with the online template creation.
	*/
int is_eye_pair(CvSeq* comp, int num, CvRect* eye)
{
	    if (comp == 0 || num != 2)
		    return 0;
 
		CvRect r1 = cvBoundingRect(comp, 1);
		comp = comp->h_next;

		if (comp == 0)
	        return 0;

	    CvRect r2 = cvBoundingRect(comp, 1);
 
    /* the width of the components are about the same */
		if (abs(r1.width - r2.width) >= 5)
			return 0;
 
    /* the height f the components are about the same */
		if (abs(r1.height - r2.height) >= 5)
			return 0;
 
    /* vertical distance is small */
		if (abs(r1.y - r2.y) >= 5)
			return 0;
 
    /* reasonable horizontal distance, based on the components' width */
		int dist_ratio = abs(r1.x - r2.x) / r1.width;
		if (dist_ratio < 2 || dist_ratio > 5)
	        return 0;
 
    /* get the centroid of the 1st component */
		CvPoint point = cvPoint(
			r1.x + (r1.width / 2),
		    r1.y + (r1.height / 2)
		);
 
    /* return eye boundaries */
		*eye = cvRect(
			point.x - (EYE_TPL_WIDTH / 2),
			point.y - (EYE_TPL_HEIGHT / 2),
			EYE_TPL_WIDTH,
			EYE_TPL_HEIGHT
		);
		return 1;
}

/*	If is_eye_pair locate & setup search window to track the eye.
	Here also tracking is being done by template matching
	while checking for the thresholded limit.
	If eye is found return true
	*/
int locate_eye(IplImage* img, IplImage* eye_template, CvRect* window, CvRect* eye)
{
	    IplImage* locate_eye_template;
		CvRect    win;
		CvPoint   minloc, maxloc, point;
		double    minval, maxval;
		int       w, h;
 
    /* get the centroid of eye */
		point = cvPoint(
			(*eye).x + (*eye).width / 2,
			(*eye).y + (*eye).height / 2
			);
 
		/*  setup search window
			replace the predefined EYE_WIN_WIDTH and EYE_WIN_HEIGHT above
			for your convenience */
		win = cvRect(
			point.x - EYE_WIN_WIDTH /2,
			point.y - EYE_WIN_HEIGHT / 2,
			EYE_WIN_WIDTH,
			EYE_WIN_HEIGHT
		);
 
		/* <ake sure that the search window is still within the frame */
		if (win.x < 0)
	        win.x = 0;
	    if (win.y < 0)
			win.y = 0;
		if (win.x + win.width > img->width)
	        win.x = img->width - win.width;
	    if (win.y + win.height > img->height)
			win.y = img->height - win.height;
 
	    /* create new image for template matching result where:
		   width  = W - w + 1, and
		height = H - h + 1 */
		w  = win.width  - eye_template->width  + 1;
		h  = win.height - eye_template->height + 1;
		locate_eye_template = cvCreateImage(cvSize(w, h), IPL_DEPTH_32F, 1);
	 
		// Apply the search window
		cvSetImageROI(img, win);
 
	    // Template matching 
		cvMatchTemplate(img, eye_template, locate_eye_template, CV_TM_SQDIFF_NORMED);
		cvMinMaxLoc(locate_eye_template, &minval, &maxval, &minloc, &maxloc, 0);
 
		// Release resources 
		cvResetImageROI(img);
		cvReleaseImage(&locate_eye_template);
	 
		// Only good matches
		if (minval > EYE_THRESHOLD)
	        return 0;
 
		// Set search window pointer
		*window = win;
 
		// Set eye pointer
		*eye = cvRect(
			win.x + minloc.x,
			win.y + minloc.y,
			EYE_TPL_WIDTH,
			EYE_TPL_HEIGHT
			); 
		return 1;
}
 
/*	Eye blink code goes here & checks for bounding rects
	to determine whether eye is blinked or not.
	*/
int is_blink(CvSeq* comp, int num, CvRect window, CvRect eye)
{
		if (comp == 0 || num != 1)
			return 0;
 
		CvRect r1 = cvBoundingRect(comp, 1);
 
		/* component is within the search window */
		if (r1.x < window.x)
			return 0;
		if (r1.y < window.y)
			return 0;
		if (r1.x + r1.width > window.x + window.width)	
			return 0;
		if (r1.y + r1.height > window.y + window.height)
			return 0;
 
		/* get the centroid of eye */
		CvPoint pt = cvPoint(
			eye.x + eye.width/2 ,
			eye.y + eye.height/2 
			);
 
		/* component is located at the eye's centroid */
		if (pt.x <= r1.x || pt.x >= r1.x + r1.width)
	        return 0;
	    if (pt.y <= r1.y || pt.y >= r1.y + r1.height)
		    return 0;

		return 1;
}

/**
 * This function provides a way to exit nicely
 * from the system. In case of error or otherwise.
 * Param : char* msg : error message to displayed
 */
void exit_nicely(char* msg)
{
    cvDestroyAllWindows();

	 /* Free memory and release all resources */
    if (capture)
        cvReleaseCapture(&capture);
    if (gray)
        cvReleaseImage(&gray);
    if (prev)
        cvReleaseImage(&prev);
    if (diff)
        cvReleaseImage(&diff);
    if (eye_template)
        cvReleaseImage(&eye_template);
	if (template_match )
		cvReleaseImage( &template_match );
    if (storage)
        cvReleaseMemStorage(&storage);
 
    if (msg != NULL)
    {
        fprintf(stderr, msg);
        fprintf(stderr, "\n");
        exit(1);
    }
 
    exit(0);
}
