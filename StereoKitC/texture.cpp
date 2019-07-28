#include "stereokit.h"
#include "texture.h"

#include "d3d.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

tex2d_t tex2d_create(const char *name) {
	return (tex2d_t)assets_allocate(asset_type_texture, name);
}
tex2d_t tex2d_create_cubemap(const char *id, const char **files, int make_mip_maps) {
	tex2d_t result = (tex2d_t)assets_find(files[0]);
	if (result != nullptr) {
		assets_addref(result->header);
		return result;
	}

	// Load all 6 faces
	uint8_t *data[6] = {};
	int  final_width  = 0;
	int  final_height = 0;
	bool loaded       = true;
	for (size_t i = 0; i < 6; i++) {
		int channels = 0;
		int width    = 0;
		int height   = 0;
		data[i] = stbi_load(files[i], &width, &height, &channels, 4);

		// Check if there were issues, or one of the images is the wrong size!
		if (data == nullptr || 
			(final_width  != 0 && final_width  != width ) ||
			(final_height != 0 && final_height != height)) {
			loaded = false;
			break;
		}
		final_width  = width;
		final_height = height;
	}

	// free memory if we failed
	if (!loaded) {
		for (size_t i = 0; i < 6; i++) {
			if (data[i] != nullptr)
				free(data[i]);
		}
		return nullptr;
	}

	// Create with the data we have
	result = tex2d_create(id);
	tex2d_set_colors_cube(result, final_width, final_height, &data[0], make_mip_maps);

	return result;
}
tex2d_t tex2d_create_file(const char *file, int make_mip_maps) {
	tex2d_t result = (tex2d_t)assets_find(file);
	if (result != nullptr) {
		assets_addref(result->header);
		return result;
	}

	int      channels = 0;
	int      width    = 0;
	int      height   = 0;
	uint8_t *data     = stbi_load(file, &width, &height, &channels, 4);

	if (data == nullptr) {
		return nullptr;
	}
	result = tex2d_create(file);

	tex2d_set_colors(result, width, height, data, make_mip_maps);
	free(data);

	return result;
}
tex2d_t tex2d_create_mem(const char *id, void *data, size_t data_size, int make_mip_maps) {
	tex2d_t result = (tex2d_t)assets_find(id);
	if (result != nullptr) {
		assets_addref(result->header);
		return result;
	}

	int      channels = 0;
	int      width    = 0;
	int      height   = 0;
	uint8_t *col_data =  stbi_load_from_memory((stbi_uc*)data, data_size, &width, &height, &channels, 4);

	if (col_data == nullptr) {
		return nullptr;
	}
	result = tex2d_create(id);

	tex2d_set_colors(result, width, height, col_data, make_mip_maps);
	free(col_data);

	return result;
}

void tex2d_release(tex2d_t texture) {
	assets_releaseref(texture->header);
}
void tex2d_destroy(tex2d_t tex) {
	if (tex->resource != nullptr) tex->resource->Release();
	if (tex->texture  != nullptr) tex->texture ->Release();
	*tex = {};
}

void tex2d_set_colors(tex2d_t texture, int width, int height, uint8_t *data_rgba32, int make_mip_maps) {
	if (texture->width != width || texture->height != height || (texture->resource != nullptr && texture->can_write == false)) {
		if (texture->resource != nullptr) {
			texture->resource->Release();
			texture->can_write = true;
		}

		D3D11_TEXTURE2D_DESC desc = {};
		desc.Width            = width;
		desc.Height           = height;
		desc.MipLevels        = 1;//(int)(log(min(width,height)) / log(2)) - 1;
		desc.ArraySize        = 1;
		desc.SampleDesc.Count = 1;
		desc.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.BindFlags        = D3D11_BIND_SHADER_RESOURCE | (make_mip_maps?D3D11_BIND_RENDER_TARGET:0);
		desc.Usage            = texture->can_write ? D3D11_USAGE_DYNAMIC    : D3D11_USAGE_DEFAULT;
		desc.CPUAccessFlags   = texture->can_write ? D3D11_CPU_ACCESS_WRITE : 0;
		desc.MiscFlags        = make_mip_maps ? D3D11_RESOURCE_MISC_GENERATE_MIPS : 0;
		if (desc.MipLevels < 1)
			desc.MipLevels = 1;

		D3D11_SUBRESOURCE_DATA data;
		data.pSysMem     = data_rgba32;
		data.SysMemPitch = sizeof(uint8_t) * 4 * width;
		data.SysMemSlicePitch = 0;

		if (FAILED(d3d_device->CreateTexture2D(&desc, &data, &texture->texture))) {
			printf("Create texture error!\n");
			return;
		}

		D3D11_SHADER_RESOURCE_VIEW_DESC res_desc = {};
		res_desc.Format              = desc.Format;
		res_desc.Texture2D.MipLevels = -1;// desc.MipLevels;
		res_desc.ViewDimension       = D3D11_SRV_DIMENSION_TEXTURE2D;
		d3d_device->CreateShaderResourceView(texture->texture, &res_desc, &texture->resource);

		if (make_mip_maps)
			d3d_context->GenerateMips(texture->resource);

		texture->width  = width;
		texture->height = height;
		return;
	}
	texture->width  = width;
	texture->height = height;

	D3D11_MAPPED_SUBRESOURCE data = {};
	if (FAILED(d3d_context->Map(texture->texture, 0, D3D11_MAP_WRITE_DISCARD, 0, &data))) {
		printf("Failed mapping a texture\n");
		return;
	}

	uint8_t *dest_line = (uint8_t*)data.pData;
	uint8_t *src_line  = data_rgba32;
	for (int i = 0; i < height; i++) {
		memcpy(dest_line, src_line, (size_t)width*sizeof(uint8_t)*4);
		dest_line += data.RowPitch;
		src_line  += width * sizeof(uint8_t) * 4;
	}
	d3d_context->Unmap(texture->texture, 0);

	if (make_mip_maps)
		d3d_context->GenerateMips(texture->resource);
}

void tex2d_set_colors_cube(tex2d_t texture, int width, int height, uint8_t **data_faces_rgba32, int make_mip_maps) {
	if (texture->resource != nullptr)
		texture->resource->Release();
	D3D11_TEXTURE2D_DESC desc = {};
	desc.Width            = width;
	desc.Height           = height;
	desc.MipLevels        = 1;// (int)(log(min(width, height)) / log(2)) - 1;
	desc.ArraySize        = 6;
	desc.SampleDesc.Count = 1;
	desc.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.BindFlags        = D3D11_BIND_SHADER_RESOURCE | (make_mip_maps?D3D11_BIND_RENDER_TARGET:0);
	desc.Usage            = D3D11_USAGE_DEFAULT;
	desc.CPUAccessFlags   = 0;
	desc.MiscFlags        = D3D11_RESOURCE_MISC_TEXTURECUBE | (make_mip_maps?D3D11_RESOURCE_MISC_GENERATE_MIPS:0);
	if (desc.MipLevels < 1)
		desc.MipLevels = 1;
	
	D3D11_SUBRESOURCE_DATA data[6];
	for (size_t i = 0; i < 6; i++) {
		data[i].pSysMem     = data_faces_rgba32[i];
		data[i].SysMemPitch = sizeof(uint8_t) * 4 * width;
		data[i].SysMemSlicePitch = 0;
	}
	
	if (FAILED(d3d_device->CreateTexture2D(&desc, &data[0], &texture->texture))) {
		printf("Create cubemap error!\n");
		return;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC res_desc = {};
	res_desc.Format              = desc.Format;
	res_desc.Texture2D.MipLevels = -1;// desc.MipLevels;
	res_desc.ViewDimension       = D3D11_SRV_DIMENSION_TEXTURECUBE;
	d3d_device->CreateShaderResourceView(texture->texture, &res_desc, &texture->resource);

	if (make_mip_maps)
		d3d_context->GenerateMips(texture->resource);

	texture->width  = width;
	texture->height = height;
}

void tex2d_set_active(tex2d_t texture, int slot) {
	if (texture != nullptr)
		d3d_context->PSSetShaderResources(slot, 1, &texture->resource);
	else
		d3d_context->PSSetShaderResources(slot, 0, nullptr);
}