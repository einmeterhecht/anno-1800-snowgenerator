#include "rdm2gl.h"
#include "snow_exception.h"
/*
Everything in this file that is not GL-specific was copied from kskudkliks rdm-obj converter.
*/


HardwareRdm::HardwareRdm()
{
    uint32_t vertices_size = 0; // byte size
    uint32_t vertices_count = 0;

    uint32_t corner_size = 0;  // byte size
    uint32_t corner_count = 0; // actual amount of triangles (COUNT IN RDM/3)

    std::vector<Material> materials;
    uint32_t materials_count = 0;

    GLuint vertexbuffer = GLuint(0);
    GLuint indexbuffer = GLuint(0);
}

int HardwareRdm::load_rdm(std::filesystem::path input_path)
{
    std::ifstream rdm_file(input_path, std::ifstream::binary);
    if (rdm_file) {
        // Read RDM file

        // Fetch length
        rdm_file.seekg(0, rdm_file.end);
        const uint64_t length = rdm_file.tellg();

        // Copy file to buffer
        rdm_file.seekg(0, rdm_file.beg);
        std::unique_ptr<char[]> file = std::make_unique<char[]>(length);
        rdm_file.read(file.get(), length);
        rdm_file.close();


        if (length < 32)
            throw snow_exception("Broken RDM file (Smaller than 32 Bytes)");

        // How to read information stored as an uint32_t from the buffer:
        // Element at offset:            file[offset]
        // Pointer to this element:      &file[offset]
        // cast pointer to uint32_t:     (uint32_t*)&file[offset]
        // Dereferentiate the pointer:   *(uint32_t*)&file[offset];

        const uintptr_t offset_to_offsets = *(uint32_t*)&file[32];

        if (length - 16 < offset_to_offsets)
            throw snow_exception("Broken RDM file (Offset behind file length)");

        const uint32_t offset_to_vertices  = *(uint32_t*)&file[offset_to_offsets + 12];
        const uint32_t offset_to_triangles = *(uint32_t*)&file[offset_to_offsets + 16];
        const uint32_t offset_to_materials = *(uint32_t*)&file[offset_to_offsets + 20];

        if (*(uint32_t*)&file[offset_to_materials - 4] != 28)
            throw snow_exception("Broken RDM file (Materialsize != 28)");
        if (length - 8 <= offset_to_vertices || length - 4 <= offset_to_triangles)
            throw snow_exception("Broken RDM file (length - 8 <= offset_to_vertices || length - 4 <= offset_to_triangles)");

        materials_count = *(uint32_t*)&file[offset_to_materials - 8];

        for (uint32_t i = 0; i < materials_count; i++) {
            materials.push_back(Material{*(uint32_t*)&file[offset_to_materials + 0 + 28 * i],
                                         *(uint32_t*)&file[offset_to_materials + 4 + 28 * i],
                                         *(uint32_t*)&file[offset_to_materials + 8 + 28 * i] });
        }

        vertices_size = *(uint32_t*)&file[offset_to_vertices - 4];
        vertices_count = *(uint32_t*)&file[offset_to_vertices - 8];

        corner_count = *(unsigned long*)&file[offset_to_triangles - 8]; // Dividing this by 3 gives the amount of triangles
        corner_size = *(unsigned long*)&file[offset_to_triangles - 4];

        // Upload vertices to GL
        glGenBuffers(1, &vertexbuffer);
        glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
        glBufferData(GL_ARRAY_BUFFER,
            vertices_count * vertices_size,
            &file[offset_to_vertices],
            GL_STATIC_DRAW);

        // Upload indices to GL
        glGenBuffers(1, &indexbuffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexbuffer);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
            corner_size * corner_count,
            &file[offset_to_triangles],
            GL_STATIC_DRAW);

        rdm_file.close();

    }
    else {
        std::cout << "Could not open " << input_path << std::endl;
        throw snow_exception("Could not open RDM file");
    }
    return 0;
}

void HardwareRdm::bind_buffers()
{
    glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexbuffer);
}

void HardwareRdm::cleanup()
{
    glDeleteBuffers(1, &vertexbuffer);
    glDeleteBuffers(1, &indexbuffer);
}

void HardwareRdm::print_information()
{
    std::cout << "Vertices size: " << vertices_size << std::endl;
    std::cout << "Vertices count: " << vertices_count << std::endl;
    std::cout << "Corner size: " << corner_size << std::endl;
    std::cout << "Corner count: " << corner_count << std::endl;
    std::cout << "Materials count: " << materials_count << std::endl;
}

GLenum HardwareRdm::get_corner_datatype()
{
    switch (corner_size) {
    case 2:
        return GL_UNSIGNED_SHORT;
        break;
    case 4:
        return GL_UNSIGNED_INT;
        break;
    default:
        std::cout << "corner_size is neither 2 or 4, but " << corner_size << std::endl;
        return GL_UNSIGNED_BYTE;
        break;
    }
}

HardwareRdm::~HardwareRdm()
{
    cleanup();
}

