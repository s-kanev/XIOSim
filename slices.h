#ifndef __SLICES_H__
#define __SLICES_H__

void start_slice(unsigned int slice_num);

void end_slice(unsigned int slice_num,
               unsigned long long slice_length,
               unsigned long long slice_weight_times_1000);

void scale_all_slices(void);

#endif /* __SLICES_H__ */
