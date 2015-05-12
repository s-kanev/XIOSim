#ifndef __FEEDER_ROI__
#define __FEEDER_ROI__

/* Check whether ROI support is needed. */
extern KNOB<BOOL> KnobROI;

/* Add instrumentation looking for ROI callbacks.
 * Will look for xiosim_roi_begin and xiosim_roi_end calls in
 * the simulated application to deliniate the region-of-interest.
 */
void AddROICallbacks(IMG img);

#endif /* __FEEDER_ROI__ */
