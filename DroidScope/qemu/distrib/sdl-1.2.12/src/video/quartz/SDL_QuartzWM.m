/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2003  Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    Sam Lantinga
    slouken@libsdl.org
*/
#include "SDL_config.h"

#include "SDL_QuartzVideo.h"
#include "SDL_QuartzWM.h"

void QZ_FreeWMCursor     (_THIS, WMcursor *cursor) { 

    if ( cursor != NULL ) {
        [ cursor->nscursor release ];
        free (cursor);
    }
}

WMcursor*    QZ_CreateWMCursor   (_THIS, Uint8 *data, Uint8 *mask, 
                                         int w, int h, int hot_x, int hot_y) { 
    WMcursor *cursor;
    NSBitmapImageRep *imgrep;
    NSImage *img;
    unsigned char *planes[5];
    int i;
    NSAutoreleasePool *pool;
    
    pool = [ [ NSAutoreleasePool alloc ] init ];
    
    /* Allocate the cursor memory */
    cursor = (WMcursor *)SDL_malloc(sizeof(WMcursor));
    if (cursor == NULL) goto outOfMemory;
    
    /* create the image representation and get the pointers to its storage */
    imgrep = [ [ [ NSBitmapImageRep alloc ] initWithBitmapDataPlanes: NULL pixelsWide: w pixelsHigh: h bitsPerSample: 1 samplesPerPixel: 2 hasAlpha: YES isPlanar: YES colorSpaceName: NSDeviceBlackColorSpace bytesPerRow: (w+7)/8 bitsPerPixel: 0 ] autorelease ];
    if (imgrep == nil) goto outOfMemory;
    [ imgrep getBitmapDataPlanes: planes ];
    
    /* copy data and mask, extending the mask to all black pixels because the inversion effect doesn't work with Cocoa's alpha-blended cursors */
    for (i = 0; i < (w+7)/8*h; i++) {
        planes[0][i] = data[i];
        planes[1][i] = mask[i] | data[i];
    }
    
    /* create image and cursor */
    img = [ [ [ NSImage alloc ] initWithSize: NSMakeSize(w, h) ] autorelease ];
    if (img == nil) goto outOfMemory;
    [ img addRepresentation: imgrep ];
    if (system_version < 0x1030) { /* on 10.2, cursors must be 16*16 */
        if (w > 16 || h > 16) { /* too big: scale it down */
            [ img setScalesWhenResized: YES ];
            hot_x = hot_x*16/w;
            hot_y = hot_y*16/h;
        }
        else { /* too small (or just right): extend it (from the bottom left corner, so hot_y must be adjusted) */
            hot_y += 16 - h;
        }
        [ img setSize: NSMakeSize(16, 16) ];
    }
    cursor->nscursor = [ [ NSCursor alloc ] initWithImage: img hotSpot: NSMakePoint(hot_x, hot_y) ];
    if (cursor->nscursor == nil) goto outOfMemory;
    
    [ pool release ];
    return(cursor);

outOfMemory:
    [ pool release ];
    if (cursor != NULL) SDL_free(cursor);
    SDL_OutOfMemory();
    return(NULL);
}

void QZ_UpdateCursor (_THIS) {
    BOOL state;

    if (cursor_should_be_visible || !(SDL_GetAppState() & SDL_APPMOUSEFOCUS)) {
        state = YES;
    } else {
        state = NO;
    }
    if (state != cursor_visible) {
        if (state) {
            [ NSCursor unhide ];
        } else {
            [ NSCursor hide ];
        }
        cursor_visible = state;
    }
}

BOOL QZ_IsMouseInWindow (_THIS) {
    if (qz_window == nil || (mode_flags & SDL_FULLSCREEN)) return YES; /*fullscreen*/
    else {
        NSPoint p = [ qz_window mouseLocationOutsideOfEventStream ];
        p.y -= 1.0f; /* Apparently y goes from 1 to h, not from 0 to h-1 (i.e. the "location of the mouse" seems to be defined as "the location of the top left corner of the mouse pointer's hot pixel" */
        return NSPointInRect(p, [ window_view frame ]);
    }
}

int QZ_ShowWMCursor (_THIS, WMcursor *cursor) { 

    if ( cursor == NULL) {
        if ( cursor_should_be_visible ) {
            cursor_should_be_visible = NO;
            QZ_ChangeGrabState (this, QZ_HIDECURSOR);
        }
        QZ_UpdateCursor(this);
    }
    else {
        if (qz_window ==nil || (mode_flags & SDL_FULLSCREEN)) {
            [ cursor->nscursor set ];
        }
        else {
            [ qz_window invalidateCursorRectsForView: [ qz_window contentView ] ];
        }
        if ( ! cursor_should_be_visible ) {
            cursor_should_be_visible = YES;
            QZ_ChangeGrabState (this, QZ_SHOWCURSOR);
        }
        QZ_UpdateCursor(this);
    }

    return 1;
}

/*
    Coordinate conversion functions, for convenience
    Cocoa sets the origin at the lower left corner of the window/screen
    SDL, CoreGraphics/WindowServer, and QuickDraw use the origin at the upper left corner
    The routines were written so they could be called before SetVideoMode() has finished;
    this might have limited usefulness at the moment, but the extra cost is trivial.
*/

/* Convert Cocoa screen coordinate to Cocoa window coordinate */
void QZ_PrivateGlobalToLocal (_THIS, NSPoint *p) {

    *p = [ qz_window convertScreenToBase:*p ];
}


/* Convert Cocoa window coordinate to Cocoa screen coordinate */
void QZ_PrivateLocalToGlobal (_THIS, NSPoint *p) {

    *p = [ qz_window convertBaseToScreen:*p ];
}

/* Convert SDL coordinate to Cocoa coordinate */
void QZ_PrivateSDLToCocoa (_THIS, NSPoint *p) {

    if ( CGDisplayIsCaptured (display_id) ) { /* capture signals fullscreen */
    
        p->y = CGDisplayPixelsHigh (display_id) - p->y;
    }
    else {
       
        *p = [ window_view convertPoint:*p toView: nil ];
        
        /* We need a workaround in OpenGL mode */
        if ( SDL_VideoSurface->flags & SDL_OPENGL ) {
            p->y = [window_view frame].size.height - p->y;
        }
    }
}

/* Convert Cocoa coordinate to SDL coordinate */
void QZ_PrivateCocoaToSDL (_THIS, NSPoint *p) {

    if ( CGDisplayIsCaptured (display_id) ) { /* capture signals fullscreen */
    
        p->y = CGDisplayPixelsHigh (display_id) - p->y;
    }
    else {

        *p = [ window_view convertPoint:*p fromView: nil ];
        
        /* We need a workaround in OpenGL mode */
        if ( SDL_VideoSurface != NULL && (SDL_VideoSurface->flags & SDL_OPENGL) ) {
            p->y = [window_view frame].size.height - p->y;
        }
    }
}

/* Convert SDL coordinate to window server (CoreGraphics) coordinate */
CGPoint QZ_PrivateSDLToCG (_THIS, NSPoint *p) {
    
    CGPoint cgp;
    
    if ( ! CGDisplayIsCaptured (display_id) ) { /* not captured => not fullscreen => local coord */
    
        int height;
        
        QZ_PrivateSDLToCocoa (this, p);
        QZ_PrivateLocalToGlobal (this, p);
        
        height = CGDisplayPixelsHigh (display_id);
        p->y = height - p->y;
    }
    
    cgp.x = p->x;
    cgp.y = p->y;
    
    return cgp;
}

#if 0 /* Dead code */
/* Convert window server (CoreGraphics) coordinate to SDL coordinate */
void QZ_PrivateCGToSDL (_THIS, NSPoint *p) {
            
    if ( ! CGDisplayIsCaptured (display_id) ) { /* not captured => not fullscreen => local coord */
    
        int height;

        /* Convert CG Global to Cocoa Global */
        height = CGDisplayPixelsHigh (display_id);
        p->y = height - p->y;

        QZ_PrivateGlobalToLocal (this, p);
        QZ_PrivateCocoaToSDL (this, p);
    }
}
#endif /* Dead code */

void  QZ_PrivateWarpCursor (_THIS, int x, int y) {
    
    NSPoint p;
    CGPoint cgp;
    
    p = NSMakePoint (x, y);
    cgp = QZ_PrivateSDLToCG (this, &p);
    
    /* this is the magic call that fixes cursor "freezing" after warp */
    CGSetLocalEventsSuppressionInterval (0.0);
    CGWarpMouseCursorPosition (cgp);
}

void QZ_WarpWMCursor (_THIS, Uint16 x, Uint16 y) {

    /* Only allow warping when in foreground */
    if ( ! [ NSApp isActive ] )
        return;
            
    /* Do the actual warp */
    if (grab_state != QZ_INVISIBLE_GRAB) QZ_PrivateWarpCursor (this, x, y);

    /* Generate the mouse moved event */
    SDL_PrivateMouseMotion (0, 0, x, y);
}

void QZ_MoveWMCursor     (_THIS, int x, int y) { }
void QZ_CheckMouseMode   (_THIS) { }

void QZ_SetCaption    (_THIS, const char *title, const char *icon) {

    if ( qz_window != nil ) {
        NSString *string;
        if ( title != NULL ) {
            string = [ [ NSString alloc ] initWithUTF8String:title ];
            [ qz_window setTitle:string ];
            [ string release ];
        }
        if ( icon != NULL ) {
            string = [ [ NSString alloc ] initWithUTF8String:icon ];
            [ qz_window setMiniwindowTitle:string ];
            [ string release ];
        }
    }
}

void QZ_SetWindowPos (_THIS, int  x, int  y)
{
    if ( qz_window == nil ) {
        //printf( "%s(%d,%d): called for NULL window\n", __FUNCTION__, x, y ); 
        return;
    }
     
    [ qz_window setFrameTopLeftPoint:NSMakePoint( x, this->hidden->height - y ) ];
    //printf( "%s(%d,%d): done\n", __FUNCTION__, x, y );
}

void  QZ_GetWindowPos(_THIS, int  *px, int  *py)
{
    NSPoint  pt;
    
    *px = *py = 0;
 
    if ( qz_window == NULL ) {
        //printf( "%s: called on NULL window\n", __FUNCTION__ );
    }

    if ( qz_window != nil ) {
        NSRect  rect = [ qz_window frame ];
        *px = rect.origin.x;
        *py = this->hidden->height - rect.origin.y - rect.size.height;
        //printf( "%s: returning (%d,%d)\n", __FUNCTION__, *px, *py );
    }
}

/* determine if the window is fully visible on the current screen configuration */
int  QZ_IsWindowVisible(_THIS, int  recenter)
{
    int  result = 0;
   
    //printf( "... enter %s\n", __FUNCTION__ );

    if ( qz_window != NULL ) {
        NSRect        frame   = [ qz_window frame ]; 
        NSArray*      screens = [ NSScreen screens ];
        unsigned int  count   = [ screens count ];
        unsigned int  n;
        //printf( "window frame (%d,%d) (%d,%d)\n", frame.origin.x, frame.origin.y,
        //        frame.size.width, frame.size.height );
        for (n = 0; n < count; n++) {
            NSScreen*  screen = [ screens objectAtIndex: n ];
            NSRect     vis    = [ screen visibleFrame ];
   
            //printf( "screen %d/%d  frame (%d,%d) (%d,%d)\n", n+1, count,
            //        vis.origin.x, vis.origin.y, vis.size.width, vis.size.height );

            if (frame.origin.x >= vis.origin.x && 
                frame.origin.x + frame.size.width <= vis.origin.x + vis.size.width &&
                frame.origin.y >= vis.origin.y &&
                frame.origin.y + frame.size.height <= vis.origin.y + vis.size.height )
            {
                result = 1;
                break;
            }
        }
    }
    //printf ( "... exit %s, result = %d\n", __FUNCTION__, result );
    if ( !result && recenter ) {
        [ qz_window center ] ;
    }
    return result;
}

int QZ_GetMonitorDPI(_THIS, int  *xDpi, int *yDpi)
{
    /* FIXME: how to get this information from Cocoa ? */
    return -1;
}

int QZ_GetMonitorRect   (_THIS, SDL_Rect  *rect)
{
    NSWindow*     window = qz_window;
    NSRect        frame   = [ window frame ];
    int           fx1     = frame.origin.x;
    int           fy1     = frame.origin.y;
    int           fx2     = frame.size.width + fx1;
    int           fy2     = frame.size.height + fy1; 
    NSArray*      screens = [ NSScreen screens ];
    unsigned int  count   = [ screens count ];
    int           bestScreen = -1;
    int           bestArea = 0;

    unsigned int  n;

    /* we need to compute which screen has the most window pixels */
    for (n = 0; n < count; n++) {
        NSScreen*  screen = [ screens objectAtIndex: n ];
        NSRect     vis    = [ screen visibleFrame ];
        int        vx1    = vis.origin.x;
        int        vy1    = vis.origin.y;
        int        vx2    = vis.size.width + vx1;
        int        vy2    = vis.size.height + vy1;
        int        cx1, cx2, cy1, cy2, cArea;

        if (fx1 >= vx2 || vx1 >= fx2 || fy1 >= vy2 || vy1 >= fy2)
            continue;

        cx1 = (fx1 < vx1) ? vx1 : fx1;
        cx2 = (fx2 > vx2) ? vx2 : fx2;
        cy1 = (fy1 < vy1) ? vy1 : fy1;
        cy2 = (fy2 > vy2) ? vy2 : fy2;

        if (cx1 >= cx2 || cy1 >= cy2)
            continue;

        cArea = (cx2-cx1)*(cy2-cy1);

        if (bestScreen < 0 || cArea > bestArea) {
            bestScreen = n;
            bestArea   = cArea;
        }
    }
    if (bestScreen < 0)
        bestScreen = 0;

    {
        NSScreen*  screen = [ screens objectAtIndex: bestScreen ];
        NSRect     vis    = [ screen visibleFrame ];

        rect->x = vis.origin.x;
        rect->y = vis.origin.y;
        rect->w = vis.size.width;
        rect->h = vis.size.height; 
    }
    return 0;
}

void QZ_SetIcon       (_THIS, SDL_Surface *icon, Uint8 *mask)
{
    NSBitmapImageRep *imgrep;
    NSImage *img;
    SDL_Surface *mergedSurface;
    NSAutoreleasePool *pool;
    Uint8 *pixels;
    SDL_bool iconSrcAlpha;
    Uint8 iconAlphaValue;
    int i, j, maskPitch, index;
    
    pool = [ [ NSAutoreleasePool alloc ] init ];
    
    imgrep = [ [ [ NSBitmapImageRep alloc ] initWithBitmapDataPlanes: NULL pixelsWide: icon->w pixelsHigh: icon->h bitsPerSample: 8 samplesPerPixel: 4 hasAlpha: YES isPlanar: NO colorSpaceName: NSDeviceRGBColorSpace bytesPerRow: 4*icon->w bitsPerPixel: 32 ] autorelease ];
    if (imgrep == nil) goto freePool;
    pixels = [ imgrep bitmapData ];
    SDL_memset(pixels, 0, 4*icon->w*icon->h); /* make the background, which will survive in colorkeyed areas, completely transparent */
    
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
#define BYTEORDER_DEPENDENT_RGBA_MASKS 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF
#else
#define BYTEORDER_DEPENDENT_RGBA_MASKS 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000
#endif
    mergedSurface = SDL_CreateRGBSurfaceFrom(pixels, icon->w, icon->h, 32, 4*icon->w, BYTEORDER_DEPENDENT_RGBA_MASKS);
    if (mergedSurface == NULL) goto freePool;
    
    /* blit, with temporarily cleared SRCALPHA flag because we want to copy, not alpha-blend */
    iconSrcAlpha = ((icon->flags & SDL_SRCALPHA) != 0);
    iconAlphaValue = icon->format->alpha;
    SDL_SetAlpha(icon, 0, 255);
    SDL_BlitSurface(icon, NULL, mergedSurface, NULL);
    if (iconSrcAlpha) SDL_SetAlpha(icon, SDL_SRCALPHA, iconAlphaValue);
    
    SDL_FreeSurface(mergedSurface);
    
    /* apply mask, source alpha, and premultiply color values by alpha */
    maskPitch = (icon->w+7)/8;
    for (i = 0; i < icon->h; i++) {
        for (j = 0; j < icon->w; j++) {
            index = i*4*icon->w + j*4;
            if (!(mask[i*maskPitch + j/8] & (128 >> j%8))) {
                pixels[index + 3] = 0;
            }
            else {
                if (iconSrcAlpha) {
                    if (icon->format->Amask == 0) pixels[index + 3] = icon->format->alpha;
                }
                else {
                    pixels[index + 3] = 255;
                }
            }
            if (pixels[index + 3] < 255) {
                pixels[index + 0] = (Uint16)pixels[index + 0]*pixels[index + 3]/255;
                pixels[index + 1] = (Uint16)pixels[index + 1]*pixels[index + 3]/255;
                pixels[index + 2] = (Uint16)pixels[index + 2]*pixels[index + 3]/255;
            }
        }
    }
    
    img = [ [ [ NSImage alloc ] initWithSize: NSMakeSize(icon->w, icon->h) ] autorelease ];
    if (img == nil) goto freePool;
    [ img addRepresentation: imgrep ];
    [ NSApp setApplicationIconImage:img ];
    
freePool:
    [ pool release ];
}

int  QZ_IconifyWindow (_THIS) { 

    if ( ! [ qz_window isMiniaturized ] ) {
        [ qz_window miniaturize:nil ];
        return 1;
    }
    else {
        SDL_SetError ("window already iconified");
        return 0;
    }
}


int  QZ_GetWMInfo  (_THIS, SDL_SysWMinfo *info) { 
    info->nsWindowPtr = qz_window;
    return 0; 
}

void QZ_ChangeGrabState (_THIS, int action) {

    /* 
        Figure out what the next state should be based on the action.
        Ignore actions that can't change the current state.
    */
    if ( grab_state == QZ_UNGRABBED ) {
        if ( action == QZ_ENABLE_GRAB ) {
            if ( cursor_should_be_visible )
                grab_state = QZ_VISIBLE_GRAB;
            else
                grab_state = QZ_INVISIBLE_GRAB;
        }
    }
    else if ( grab_state == QZ_VISIBLE_GRAB ) {
        if ( action == QZ_DISABLE_GRAB )
            grab_state = QZ_UNGRABBED;
        else if ( action == QZ_HIDECURSOR )
            grab_state = QZ_INVISIBLE_GRAB;
    }
    else {
        assert( grab_state == QZ_INVISIBLE_GRAB );
        
        if ( action == QZ_DISABLE_GRAB )
            grab_state = QZ_UNGRABBED;
        else if ( action == QZ_SHOWCURSOR )
            grab_state = QZ_VISIBLE_GRAB;
    }
    
    /* now apply the new state */
    if (grab_state == QZ_UNGRABBED) {
    
        CGAssociateMouseAndMouseCursorPosition (1);
    }
    else if (grab_state == QZ_VISIBLE_GRAB) {
    
        CGAssociateMouseAndMouseCursorPosition (1);
    }
    else {
        assert( grab_state == QZ_INVISIBLE_GRAB );

        QZ_PrivateWarpCursor (this, SDL_VideoSurface->w / 2, SDL_VideoSurface->h / 2);
        CGAssociateMouseAndMouseCursorPosition (0);
    }
}

SDL_GrabMode QZ_GrabInput (_THIS, SDL_GrabMode grab_mode) {

    int doGrab = grab_mode & SDL_GRAB_ON;
    /*int fullscreen = grab_mode & SDL_GRAB_FULLSCREEN;*/

    if ( this->screen == NULL ) {
        SDL_SetError ("QZ_GrabInput: screen is NULL");
        return SDL_GRAB_OFF;
    }
        
    if ( ! video_set ) {
        /*SDL_SetError ("QZ_GrabInput: video is not set, grab will take effect on mode switch"); */
        current_grab_mode = grab_mode;
        return grab_mode;       /* Will be set later on mode switch */
    }

    if ( grab_mode != SDL_GRAB_QUERY ) {
        if ( doGrab )
            QZ_ChangeGrabState (this, QZ_ENABLE_GRAB);
        else
            QZ_ChangeGrabState (this, QZ_DISABLE_GRAB);
        
        current_grab_mode = doGrab ? SDL_GRAB_ON : SDL_GRAB_OFF;
    }

    return current_grab_mode;
}
