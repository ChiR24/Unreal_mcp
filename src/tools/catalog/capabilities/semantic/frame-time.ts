import { z } from 'zod';

export const FrameRangeSchema = z
  .strictObject({
    startFrame: z.number().int(),
    endFrame: z.number().int()
  })
  .readonly()
  .refine((range) => range.endFrame >= range.startFrame, {
    message: 'endFrame must be greater than or equal to startFrame',
    path: ['endFrame']
  });
export type FrameRange = z.infer<typeof FrameRangeSchema>;

export const TimeRangeSchema = z
  .strictObject({
    startMs: z.number().nonnegative(),
    endMs: z.number().nonnegative()
  })
  .readonly()
  .refine((range) => range.endMs >= range.startMs, {
    message: 'endMs must be greater than or equal to startMs',
    path: ['endMs']
  });
export type TimeRange = z.infer<typeof TimeRangeSchema>;

export function parseFrameRange(input: unknown): FrameRange {
  return FrameRangeSchema.parse(input);
}

export function parseTimeRange(input: unknown): TimeRange {
  return TimeRangeSchema.parse(input);
}
