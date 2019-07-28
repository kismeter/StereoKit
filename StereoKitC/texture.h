#pragma once

#include <d3d11.h>

#include "stereokit.h"
#include "assets.h"

struct _tex2d_t {
	asset_header_t header;
	int width;
	int height;
	bool can_write;
	ID3D11ShaderResourceView *resource;
	ID3D11Texture2D          *texture;
};

tex2d_t tex2d_create_mem(const char *id, void *data, size_t data_size, int make_mip_maps = true);
void    tex2d_set_active(tex2d_t texture, int slot);
void    tex2d_destroy   (tex2d_t texture);