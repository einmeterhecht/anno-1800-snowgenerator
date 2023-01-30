#include "dds2gl.h"

#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>

#include <d3d11.h>
#include <wrl/client.h>

#include "snow_exception.h"


std::wstring string_to_16bit_unicode_wstring(std::string input_string) {
	std::wstring output_string = std::wstring();
	uint16_t int_representation;
	for (int i=0; i<input_string.size(); i++) {
		char char_to_convert = input_string[i];
		int_representation = int(char_to_convert);
		output_string += std::wstring(1, *(wchar_t*)&int_representation);
	}
	// std::wcout << L"Converted to " << output_string << std::endl;
	return output_string;
}

const wchar_t* GetErrorDesc(HRESULT hr) // Code copied from Texconv.cpp (MIT-license) (https://github.com/microsoft/DirectXTex/blob/main/Texconv/texconv.cpp)
{
	static wchar_t desc[1024] = {};

	LPWSTR errorText = nullptr;

	DWORD result = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_ALLOCATE_BUFFER,
		nullptr, static_cast<DWORD>(hr),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPWSTR>(&errorText), 0, nullptr);

	*desc = 0;

	if (result > 0 && errorText)
	{
		swprintf_s(desc, L": %ls", errorText);

		size_t len = wcslen(desc);
		if (len >= 2)
		{
			desc[len - 2] = 0;
			desc[len - 1] = 0;
		}

		if (errorText)
			LocalFree(errorText);
	}

	return desc;
}

GLuint directx_image_to_gl_texture(const DirectX::Image* image) {
	GLuint texture_id;
	glGenTextures(1, &texture_id);
	glBindTexture(GL_TEXTURE_2D, texture_id);

	// What to do with texture coordinates outside image
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	// What to do when scaling the image
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	glTexImage2D(
		GL_TEXTURE_2D, 0, // Target
		GL_RGBA8, image->width, image->height, 0, // How to store the image
		GL_RGBA, GL_UNSIGNED_BYTE, image->pixels); // The data from DirectX

	glfwPollEvents();

	int width, height;
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);

	return texture_id;
}

GLuint dds_file_to_gl_texture(std::wstring dds_filepath) {
	DirectX::TexMetadata info;
	std::unique_ptr<DirectX::ScratchImage> scratchimage(new (std::nothrow) DirectX::ScratchImage);

	glfwPollEvents();

	long hr = DirectX::LoadFromDDSFile(dds_filepath.c_str(), DirectX::DDS_FLAGS_ALLOW_LARGE_FILES, &info, *scratchimage);
	
	glfwPollEvents();
	
	if ((hr)){
		scratchimage->Release();
		std::wcout << "Could not load from " << dds_filepath << std::endl;
		throw snow_exception("Texture loading from dds file failed...");
	}
	else {
	    auto image = scratchimage->GetImage(0, 0, 0);
	    /*
		std::cout << "Texture_width: " << image->width << std::endl;
	    std::cout << "Texture_height: " << image->height << std::endl;
		std::cout << "Texture_format: " << image->format << std::endl; // Should be '98' (=DXGI_FORMAT_BC7_UNORM)
		std::cout << "Texture_rowPitch: " << image->rowPitch << std::endl;
		std::cout << "Texture_slicePitch: " << image->slicePitch << std::endl;
		*/

		uint32_t pixel_count = image->width * image->height;

		// We want to convert the image to DXGI_FORMAT_R8G8B8A8_UINT (='30')
		std::unique_ptr<DirectX::ScratchImage> extracted_scratchimage(new (std::nothrow) DirectX::ScratchImage);
		hr = Decompress(image, scratchimage->GetImageCount(), info, DXGI_FORMAT_R8G8B8A8_UNORM, *extracted_scratchimage);

		glfwPollEvents();

		scratchimage->Release();

		if (FAILED(hr)) {
			extracted_scratchimage->Release();
			throw snow_exception("Texture extraction failed...");
		}
		else {
			const DirectX::Image* extracted_image = extracted_scratchimage->GetImage(0, 0, 0);
			GLuint texture_id = directx_image_to_gl_texture(extracted_image);
			extracted_scratchimage->Release();
			return texture_id;
			
			/*
			// Code to copy pixel data from DirectX to a custom buffer

			// Allocate buffer space for storing the image (4 bytes per pixel)
			uint8_t* image_buffer = new uint8_t[pixel_count * 4];
			// Pointer specifying the pixel to copy
			uint8_t* pixel_pointer = extracted_image->pixels;
			for (int i = 0; i < pixel_count * 4; i++) {
				image_buffer[i] = *pixel_pointer; // Copy pixel
				pixel_pointer++; // Move pointer to the next pixel
			}

			// Using the image...
			
			delete[] image_buffer; // Free disk space
			*/
		}
	}
	return 0;
}


int gl_texture_to_png_file(GLuint texture_id, std::filesystem::path filename, boolean append_extension)
{
    if (append_extension){
		if (!(filename.string().ends_with(".png") || filename.string().ends_with(".PNG"))){
			filename = filename.concat(".png");
		}
	}

	glBindTexture(GL_TEXTURE_2D, texture_id);

	int width, height;
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);
	if (glGetError() != GL_NO_ERROR) std::cout << "GL ERROR while trying to get texture informations" << std::endl;
	uint32_t pixel_count = width * height;
	uint8_t* image_buffer = new uint8_t[pixel_count * 4];
	glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_buffer);
	if (glGetError() != GL_NO_ERROR) std::cout << "GL ERROR while trying to get texture data" << std::endl;

	DirectX::Image image{};
	image.width = width;
	image.height = height;
	image.pixels = image_buffer;
	image.format = DXGI_FORMAT_R8G8B8A8_UNORM;
	image.rowPitch = image.width * 4;
	image.slicePitch = pixel_count * 4;

	std::filesystem::create_directories(filename.parent_path());
	
	long hr = DirectX::SaveToWICFile(image, DirectX::WIC_FLAGS_NONE, DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG), filename.c_str());
	delete[] image.pixels;

	if (FAILED(hr)){
		std::cout << "Could not save to \"" << filename.string() << "\"" << std::endl;
		std::wcout << static_cast<unsigned int>(hr) << std::wstring(GetErrorDesc(hr)) << std::endl;
		throw snow_exception("Texture saving failed");
	    return 1;
	}
	else {
		std::cout << "Texture successfully saved to " << filename.string() << std::endl;
		return 0;
	}
}

Microsoft::WRL::ComPtr<ID3D11Device> get_d3d11_device() {
	// https://docs.microsoft.com/en-us/windows/win32/direct3dgetstarted/work-with-dxgi

	D3D_FEATURE_LEVEL feature_levels[] = {
		D3D_FEATURE_LEVEL_10_0, // Minimal requirement for DirectCompute (Needed to compress images to BC7_UNORM)
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_11_1
	};

	Microsoft::WRL::ComPtr<ID3D11Device>        device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
	D3D_FEATURE_LEVEL m_featureLevel;

	long hr = D3D11CreateDevice(
		nullptr,                    // Specify nullptr to use the default adapter.
		D3D_DRIVER_TYPE_HARDWARE,   // Create a device using the hardware graphics driver.
		0,                          // Should be 0 unless the driver is D3D_DRIVER_TYPE_SOFTWARE.
		D3D11_CREATE_DEVICE_BGRA_SUPPORT, // Flags
		feature_levels,             // List of feature levels this app can support.
		ARRAYSIZE(feature_levels),  // Size of the list above.
		D3D11_SDK_VERSION,          // Always set this to D3D11_SDK_VERSION for Windows Store apps.
		&device,                    // Returns the Direct3D device created.
		&m_featureLevel,            // Returns feature level of device created.
		&context                    // Returns the device immediate context.
	);

	if (FAILED(hr))
	{
		throw snow_exception("Could not get access to the graphics card.");
	}
	return device;
}

void gl_texture_to_dds_mipmaps(GLuint texture_id, std::filesystem::path filename_until_mipmap_indication, size_t mipmap_count)
{
	// In case of errors: Do not throw an exception, but just return without saving the texture.
	if (mipmap_count == 0) return;
	std::cout << "Mipmap count: " << std::to_string(mipmap_count) << std::endl;

	// Update the window from time to time (Otherwise it won't react for some seconds)
	glfwPollEvents();

	glBindTexture(GL_TEXTURE_2D, texture_id);

	int width, height;
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);
	if (glGetError() != GL_NO_ERROR) std::cout << "GL ERROR while trying to get texture informations" << std::endl;
	size_t pixel_count = width * height;
	BYTE* image_buffer = (BYTE*)malloc(pixel_count*4);
	glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_buffer);
	if (glGetError() != GL_NO_ERROR) std::cout << "GL ERROR while trying to get texture data" << std::endl;

	if (mipmap_count == 1 && width >= 32 && height >= 32) mipmap_count = 4; // Generate mipmaps also if the original did not have them

	DirectX::Image image{};
	image.width = width;
	image.height = height;
	image.pixels = image_buffer;
	image.format = DXGI_FORMAT_R8G8B8A8_UNORM;
	image.rowPitch = image.width * 4;
	image.slicePitch = pixel_count * 4;

	std::unique_ptr<DirectX::ScratchImage> mipmaps(new (std::nothrow) DirectX::ScratchImage);
	long hr; // Stores error codes of DirectX operations

	if (mipmap_count == 1) {
		mipmaps->InitializeFromImage(image);
	}
	else {
		hr = DirectX::GenerateMipMaps(image, DirectX::TEX_FILTER_LINEAR, mipmap_count, *mipmaps);

		if (FAILED(hr)) {
			mipmaps->Release();
			delete[] image.pixels;
			std::wcout << static_cast<unsigned int>(hr) << std::wstring(GetErrorDesc(hr)) << std::endl;
			std::cerr << "WARNING: Could not generate mipmaps for " << filename_until_mipmap_indication.string() << "0.dds" << std::endl;
			std::cout << "This texture won't be saved." << std::endl;
			return;
		}
	}

	// Update the window from time to time (Otherwise it won't react for some seconds)
	glfwPollEvents();

	// Get access to the graphics card for compressing the image
	Microsoft::WRL::ComPtr<ID3D11Device> device = get_d3d11_device();
	std::unique_ptr<DirectX::ScratchImage> compressed_mipmaps(new (std::nothrow) DirectX::ScratchImage);
	hr = DirectX::Compress(
		device.Get(),
		mipmaps->GetImages(),
		mipmaps->GetImageCount(),
		mipmaps->GetMetadata(),
		DXGI_FORMAT_BC7_UNORM,
		DirectX::TEX_COMPRESS_DEFAULT,
		1.0f,
		*compressed_mipmaps);
	mipmaps->Release();
	delete[] image.pixels;
	if (FAILED(hr)) {
		compressed_mipmaps->Release();
		std::wcout << static_cast<unsigned int>(hr) << std::wstring(GetErrorDesc(hr)) << std::endl;
		std::cerr << "Could not compress to BC7_UNORM...\nTexture path: " << filename_until_mipmap_indication << "0.dds" << std::endl;
		std::cout << "This texture won't be saved." << std::endl;
		return;
	}
	std::filesystem::create_directories(filename_until_mipmap_indication.parent_path());
	for (size_t i = 0; i < mipmap_count; i++) {

		// Update the window from time to time (Otherwise it won't react for some seconds)
		glfwPollEvents();

		std::filesystem::path full_out_path = std::filesystem::path(filename_until_mipmap_indication).concat(
			std::to_string(i)).concat(".dds");
		long hr = DirectX::SaveToDDSFile(
			*compressed_mipmaps->GetImage(i, 0, 0),
			DirectX::DDS_FLAGS_ALLOW_LARGE_FILES,
			full_out_path.wstring().c_str());

		if (FAILED(hr)) {
			std::cout << "WARNING: Could not save to \"" << filename_until_mipmap_indication << "\"" << std::endl;
			std::wcout << static_cast<unsigned int>(hr) << std::wstring(GetErrorDesc(hr)) << std::endl;
		}
		else {
			
		}
	}
	std::cout << "Saved " << mipmap_count << " miplevels for " << filename_until_mipmap_indication.string() << std::endl;
	compressed_mipmaps->Release();
}
