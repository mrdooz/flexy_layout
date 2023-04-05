#include "raylib.h"

#pragma warning(push)
#pragma warning(disable : 4996)
#pragma warning(disable : 4115)
#pragma warning(disable : 4244)
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#pragma warning(pop)

#include "flexy_layout.hpp"

static int num_boxes = 3;
static int container_alignment = 0;
static float container_size = 500;
static bool flex_grow = false;
static bool flex_shrink = false;
static bool wrap = false;
static int direction = 0;
static int item_alignment = 0;
static int multi_row_alignment = 0;

static bool image_created = false;

static Image test_image;
static Texture2D test_texture;
static char* pixels;

//---------------------------------------------------------------------------------------
template <typename T>
T GuiSliderT(Rectangle bounds, const char* text, T value, T minValue, T maxValue)
{
    // Helper function because I was scared of casting ints to float willy-nilly :)
    float tmp_value = (float)value;
    float tmp_min_value = (float)minValue;
    float tmp_max_value = (float)maxValue;

    float text_size = (float)GetTextWidth(text) + 100;
    Rectangle r2 = bounds;
    r2.width -= text_size;
    float new_value =
        GuiSliderPro(r2, NULL, text, tmp_value, tmp_min_value, tmp_max_value, GuiGetStyle(SLIDER, SLIDER_WIDTH));
    return (T)new_value;
}

//---------------------------------------------------------------------------------------
void gui_test()
{
    {
        float gui_control_width = 600;
        float gui_contol_height = 800;
        int screen_width = GetScreenWidth();

        // First create a layout for the gui controls
        flexy::layout_t gui_layout({
            .x = screen_width - gui_control_width - 20,
            .y = 20,
            .width = gui_control_width,
            .height = gui_contol_height,
        });

        int gui_container = gui_layout.add_item({
            .width = gui_control_width,
            .height = gui_contol_height,
            .horizontal = false,
        });

        // Push a common config used by the gui items
        gui_layout.push_config({
            .parent_id = gui_container,
            .width = gui_control_width,
            .height = 30.f,
            .margin = flexy::margin_t{6, 0, 0, 0},
            .padding = flexy::padding_t{0, 0, 0, 100},
            .merge_config = false,
        });

        // add_item will now use the common config
        gui_layout.add_item({
            .render_callback =
                [=](void* userdata, const Rectangle& rect) {
                    num_boxes = GuiSliderT(rect, TextFormat("NUM BOXES: %d", num_boxes), num_boxes, 0, 20);
                },
        });

        gui_layout.add_item({
            .render_callback =
                [=](void* userdata, const Rectangle& rect) {
                    container_alignment = GuiSliderT(
                        rect, TextFormat("CONTAINER ALIGN: %d", container_alignment), container_alignment, 0, 5);
                },
        });

        gui_layout.add_item({
            .render_callback =
                [=](void* userdata, const Rectangle& rect) {
                    container_size = GuiSliderT(
                        rect, TextFormat("CONTAINER SIZE: %f", container_size), container_size, 100.f, 1000.f);
                },
        });

        // Push a container for the checkboxes, and tell it not to use the existing config
        int cb_container = gui_layout.add_item({
            .parent_id = gui_container,
            .width = gui_control_width,
            .height = 30.f,
            .horizontal = true,
            .use_config_stack = false,
        });

        // Push a new config for the checkboxes, and tell it not to merge with the existing config
        gui_layout.push_config({
            .parent_id = cb_container,
            .width = 30.f,
            .height = 30.f,
            .margin = flexy::margin_t{2, 100, 0, 0},
            .merge_config = false,
        });

        gui_layout.add_item({
            .margin = flexy::margin_t{2, 100, 0, 100},
            .render_callback = [=](void* userdata,
                                   const Rectangle& rect) { flex_grow = GuiCheckBox(rect, "FLEX-GROW", flex_grow); },
        });

        gui_layout.add_item({
            .render_callback =
                [=](void* userdata, const Rectangle& rect) {
                    flex_shrink = GuiCheckBox(rect, "FLEX-SHRINK", flex_shrink);
                },
        });

        gui_layout.add_item({
            .render_callback = [=](void* userdata, const Rectangle& rect) { wrap = GuiCheckBox(rect, "WRAP", wrap); },
        });

        // Done with the checkbox config, so pop it
        gui_layout.pop_config();

        // Go back to adding items with the old config
        gui_layout.add_item({
            .render_callback =
                [=](void* userdata, const Rectangle& rect) {
                    direction = GuiSliderT(rect, TextFormat("Direction: %d", direction), direction, 0, 1);
                },
        });
        gui_layout.add_item({
            .render_callback =
                [=](void* userdata, const Rectangle& rect) {
                    item_alignment =
                        GuiSliderT(rect, TextFormat("ITEM ALIGN: %d", item_alignment), item_alignment, 0, 3);
                },
        });

        // Add a default item, store its id, and use this later to fetch the rectangle and render it
        int mra_id = gui_layout.add_item({});

        gui_layout.pop_config();

        gui_layout.do_layout();

        // Use the id to fecth the item rectangle
        multi_row_alignment = GuiSliderT(
            gui_layout.get_rect_for_item(mra_id),
            TextFormat("MULTI-ROW ALIGN: %d", multi_row_alignment),
            multi_row_alignment,
            0,
            6);
    }

    {
        // Next create a layout where we draw the boxes and the texture
        flexy::layout_t layout({
            .x = 20,
            .y = 20,
            .width = container_size,
            .height = 2 * container_size,
        });

        bool horizontal = direction == 0;

        int c1 = layout.add_item({
            .width = container_size,
            .height = 2 * container_size,
            .horizontal = false,
            .wrap = false,
        });

        int c0 = layout.add_item({
            .parent_id = c1,
            .width = container_size,
            .height = container_size,
            .horizontal = horizontal,
            .wrap = wrap,
            .container_alignment = flexy::container_alignment_t(container_alignment),
            .multi_row_alignment = flexy::container_alignment_t(multi_row_alignment),
            .item_alignment = flexy::item_alignment_t(item_alignment),
            .render_callback = [](void* userdata, const Rectangle& rect) { DrawRectangleRec(rect, BLUE); },
        });

        static Color colors[] = {YELLOW, GOLD, ORANGE, PINK, RED, GREEN};

        for (int i = 0; i < num_boxes; ++i) {
            Color col = colors[i % 6];
            layout.add_item({
                .parent_id = c0,
                .width = 90.f,
                .height = float(60 + 20 * i),
                .flex_grow = flex_grow ? 1 : 0,
                .flex_shrink = flex_shrink ? 1 : 0,
                .margin = flexy::margin_t{5, 5, 0, 5},
                .render_callback = [=](void* userdata, const Rectangle& rect) { DrawRectangleRec(rect, col); },
            });
        }

        // This is just me testing out how to update a Raylib texture at runtime.. feel free to ignore :)
        if (!image_created) {
            pixels = new char[500 * 500 * 4];
            memset(pixels, 0, 500 * 500 * 4);

            test_image = GenImageColor(500, 500, BLACK);
            // set the image's format so it is guaranteed to be aligned with our pixel buffer format below
            ImageFormat(&test_image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

            test_texture = LoadTextureFromImage(test_image);
            image_created = true;
        }

        for (int i = 0; i < 500; ++i) {
            for (int j = 0; j < 500; ++j) {
                // 0 = R, 1 = G, 2 = B, 3 = A
                pixels[(i * 500 + j) * 4 + 0] = (char)(255.f * num_boxes * j / 500);
                pixels[(i * 500 + j) * 4 + 3] = (char)0xff;
            }
        }

        layout.add_item({
            .parent_id = c1,
            .width = 500.f,
            .height = 500.f,
            .render_callback =
                [=](void* userdata, const Rectangle& rect) {
                    UpdateTexture(test_texture, pixels);
                    DrawTexture(test_texture, (int)rect.x, (int)rect.y, WHITE);
                },
        });

        layout.do_layout();
    }
}

//---------------------------------------------------------------------------------------
int main(void)
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1600, 1200, "raylib [core] example - basic window");
    SetWindowMinSize(1200, 1000);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);
        gui_test();
        EndDrawing();
    }

    CloseWindow();

    return 0;
}
