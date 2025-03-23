#include <filesystem>
#include <fstream>
#include <cstdio>
#include <vector>
#include <cstdint>

std::vector<uint8_t> read_file(const char* path) {
    std::ifstream input_file{path, std::ios::binary};
    std::vector<uint8_t> ret{};

    if (!input_file.good()) {
        return ret;
    }

    // Get the length of the file
    input_file.seekg(0, std::ios::end);
    ret.resize(input_file.tellg());
    
    // Read the file contents into the vector
    input_file.seekg(0, std::ios::beg);
    input_file.read(reinterpret_cast<char*>(ret.data()), ret.size());

    return ret;
}

void create_parent_if_needed(const char* path) {
    std::filesystem::path parent_path = std::filesystem::path{path}.parent_path();
    if (!parent_path.empty()) {
        std::filesystem::create_directories(parent_path);
    }
}

int main(int argc, const char** argv) {
    if (argc != 5) {
        printf("Usage: %s [input file] [array name] [output C file] [output C header]\n", argv[0]);
        return EXIT_SUCCESS;
    }

    const char* input_path = argv[1];
    const char* array_name = argv[2];
    const char* output_c_path = argv[3];
    const char* output_h_path = argv[4];

    // Read the input file's contents
    std::vector<uint8_t> contents = read_file(input_path);

    if (contents.empty()) {
        fprintf(stderr, "Failed to open file %s! (Or it's empty)\n", input_path);
        return EXIT_FAILURE;
    }

    // Create the output directories if they don't exist
    create_parent_if_needed(output_c_path);
    create_parent_if_needed(output_h_path);

    // Define the size variable name
    std::string size_name = std::string(array_name) + "Size";

    // Write the C file with the array and its size
    {
        std::ofstream output_c_file{output_c_path};
        output_c_file << "#include <stdint.h>\n\n";
        output_c_file << "extern const uint8_t " << array_name << "[" << contents.size() << "];\n";
        output_c_file << "extern const uint32_t " << size_name << ";\n\n";
        
        output_c_file << "const uint8_t " << array_name << "[" << contents.size() << "] = {";

        for (uint8_t x : contents) {
            output_c_file << (int)x << ", ";
        }

        output_c_file << "};\n\n";
        output_c_file << "const uint32_t " << size_name << " = " << contents.size() << ";\n";
    }

    // Write the header file with the extern array and size declarations
    {
        std::ofstream output_h_file{output_h_path};
        output_h_file << "// Generated file - do not edit\n";
        output_h_file << "#ifndef _" << std::filesystem::absolute(output_h_path).string() << '\n';
        output_h_file << "#define _" << std::filesystem::absolute(output_h_path).string() << '\n';
        output_h_file << "\n#include <stdint.h>\n\n";
        output_h_file << "extern const uint8_t " << array_name << "[];\n";
        output_h_file << "extern const uint32_t " << size_name << ";\n\n";
        output_h_file << "#endif // _" << std::filesystem::absolute(output_h_path).string() << '\n';
    }
    
    return EXIT_SUCCESS;
}
