#include "feedback_level_seed.h"

int main(void)
{
	bool pending = true;
	float level = feedback_level_seed_update(1000.0f, 3000.0f, 0.1f, &pending, false);
	if (!pending || level != 1200.0f)
		return 1;
	level = feedback_level_seed_update(level, 2048.0f, 0.1f, &pending, true);
	if (pending || level != 2048.0f)
		return 2;
	level = feedback_level_seed_update(level, 2250.0f, 0.1f, &pending, true);
	if (pending || level != 2160.0f)
		return 3;
	pending = true;
	level = feedback_level_seed_update(level, 2100.0f, 0.1f, &pending, true);
	return pending || level != 2100.0f ? 4 : 0;
}
