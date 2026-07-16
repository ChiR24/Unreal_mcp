import { z } from 'zod';

const MAX_PAGE_SIZE = 200;

export const PaginationSchema = z
  .strictObject({
    page: z.number().int().positive(),
    pageSize: z.number().int().positive().max(MAX_PAGE_SIZE)
  })
  .readonly();
export type Pagination = z.infer<typeof PaginationSchema>;

export function parsePagination(input: unknown): Pagination {
  return PaginationSchema.parse(input);
}
