#pragma once

#include <stdint.h>
#include <functional>
#include <optional>

struct Rectangle;

// Based on flexbox, but simplified
namespace flexy {

    // Used to lay out items and rows in a container
    enum class container_alignment_t {
        start,
        end,
        center,
        space_between,  // the empty space is between the middle items
        space_around,   // half empty space on each side, and equal empty between the middle items
        space_evenly,   // equal empty space between all items (including the first/last)
        stretch,        // (only used in multi-row)
    };

    // Used to lay out individual items
    enum class item_alignment_t {
        start,
        end,
        center,
        stretch,
    };

    // Margin - space between the items. Isn't considered part of an item's size
    struct margin_t
    {
        float top = 0;
        float right = 0;
        float bottom = 0;
        float left = 0;
    };

    // Padding - space between the item's border, and where the content starts. Part of an item's size.
    struct padding_t
    {
        float top = 0;
        float right = 0;
        float bottom = 0;
        float left = 0;
    };

    using id = int32_t;

    //=======================================================================================
    struct add_item_cfg_t;
    struct layout_t
    {
        layout_t(const Rectangle& layout_rect);
        ~layout_t();

        void push_config(const add_item_cfg_t& cfg);
        void pop_config();

        id add_item(const add_item_cfg_t& cfg);
        void do_layout();

        Rectangle get_rect_for_item(id item_id) const;

        struct private_t;
        private_t* p_ = nullptr;
    };

    //=======================================================================================
    using render_callback_t = std::function<void(void* userdata, const Rectangle& rect)>;

    // Configuration when adding a new item
    struct add_item_cfg_t
    {
        std::optional<int> parent_id;

        std::optional<float> width;
        std::optional<float> height;

        std::optional<float> min_width;
        std::optional<float> min_height;

        std::optional<float> max_width;
        std::optional<float> max_height;

        std::optional<int> flex_grow;    // Relative growth factor between items when there is free space
        std::optional<int> flex_shrink;  // Relative shrink factor, when the items exceed the container size
        std::optional<margin_t> margin;
        std::optional<padding_t> padding;

        // Used when the item itself is a container (ie has children)
        std::optional<bool> horizontal;
        std::optional<bool> wrap;
        std::optional<container_alignment_t> container_alignment = container_alignment_t::start;
        std::optional<container_alignment_t> multi_row_alignment = container_alignment_t::start;
        std::optional<item_alignment_t> item_alignment = item_alignment_t::start;

        std::optional<void*> userdata;  // Passed as part of the render_callback
        std::optional<render_callback_t> render_callback;

        bool merge_config = true;      // If pushing a config, should this be merged with the existing config?
        bool use_config_stack = true;  // Should we use the existing config stack, or only use the given config?
    };

    //=======================================================================================
    struct scoped_config_t
    {
        scoped_config_t(const add_item_cfg_t& cfg, layout_t* layout);
        ~scoped_config_t();

        layout_t* layout;
    };

}  // namespace flexy
