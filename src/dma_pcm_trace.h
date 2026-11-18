#ifndef DMA_PCM_TRACE_H
#define DMA_PCM_TRACE_H

#include <stdbool.h>

typedef enum
{
	DMA_PCM_TRACE_NONE,
	DMA_PCM_TRACE_START,
	DMA_PCM_TRACE_END,
	DMA_PCM_TRACE_ABORT
} dma_pcm_trace_event_t;

static inline dma_pcm_trace_event_t dma_pcm_trace_update(bool *active, bool next_pcm, bool aborted)
{
	if (*active == next_pcm)
		return DMA_PCM_TRACE_NONE;
	*active = next_pcm;
	if (next_pcm)
		return DMA_PCM_TRACE_START;
	return aborted ? DMA_PCM_TRACE_ABORT : DMA_PCM_TRACE_END;
}

#endif
