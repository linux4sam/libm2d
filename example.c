#include "m2d.h"

static int fill()
{
	void* handle;
	struct m2d_surface surface;
	int ret = 0;

	if (m2d_open(&handle) != 0)
	{
		return -1;
	}

	//surface.clrcolor = color | 0xFF000000;
	surface.x   = 0;
	surface.y    = 0;
	surface.width  = 100;
	surface.height= 100;

	uint32_t rgb = 0xffffffff;

	if (m2d_fill(handle, rgb, &surface))
	{
		ret = -1;
	}

	if (m2d_flush(handle) != 0)
	{
		ret = -1;
	}

abort:
	if (m2d_close(handle) != 0)
	{

		ret = -1;
	}

	return ret;

}

static int copy()
{
	void* handle;
	struct m2d_surface src;
	struct m2d_surface dst;
	int ret = 0;

	if (m2d_open(&handle) != 0)
	{
		return -1;
	}

	src.x   = 0;
	src.y    = 0;
	src.width  = 100;
	src.height= 100;

	dst.x   = 0;
	dst.y    = 0;
	dst.width  = 100;
	dst.height= 100;

	if (m2d_copy(handle, &src, &dst))
	{
		ret = -1;
	}

	if (m2d_flush(handle) != 0)
	{
		ret = -1;
	}

abort:
	if (m2d_close(handle) != 0)
	{

		ret = -1;
	}

	return ret;

}

int main()
{
	fill();
	return 0;
}
