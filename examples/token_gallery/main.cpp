#include "token_gallery_definition.hpp"

#include <utility>

int main(int argc, char** argv) {
    return rynui::example::run_token_gallery(
        argc, argv, rynui::example::make_token_gallery_definition());
}
