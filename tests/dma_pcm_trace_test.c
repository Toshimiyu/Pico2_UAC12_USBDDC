#include "dma_pcm_trace.h"

int main(void)
{
	bool active = false;
	if (dma_pcm_trace_update(&active, false, false) != DMA_PCM_TRACE_NONE)
		return 1;
	if (dma_pcm_trace_update(&active, true, false) != DMA_PCM_TRACE_START || !active)
		return 2;
	if (dma_pcm_trace_update(&active, true, false) != DMA_PCM_TRACE_NONE)
		return 3;
	if (dma_pcm_trace_update(&active, false, false) != DMA_PCM_TRACE_END || active)
		return 4;
	if (dma_pcm_trace_update(&active, false, true) != DMA_PCM_TRACE_NONE)
		return 5;
	if (dma_pcm_trace_update(&active, true, false) != DMA_PCM_TRACE_START)
		return 6;
	if (dma_pcm_trace_update(&active, false, true) != DMA_PCM_TRACE_ABORT || active)
		return 7;
	if (dma_pcm_trace_update(&active, true, false) != DMA_PCM_TRACE_START || !active)
		return 8;
	return 0;
}
