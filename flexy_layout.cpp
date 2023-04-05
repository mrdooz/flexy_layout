#include "flexy_layout.hpp"

#include <assert.h>
#include <raylib.h>  // For Rectangle and scissoring

namespace flexy {

    //---------------------------------------------------------------------------------------
    template <typename T>
    T flexy_min(T a, T b)
    {
        return a < b ? a : b;
    }

    //---------------------------------------------------------------------------------------
    template <typename T>
    T flexy_max(T a, T b)
    {
        return a > b ? a : b;
    }

    //---------------------------------------------------------------------------------------
    template <typename T>
    T flexy_clamp(T v, T lo, T hi)
    {
        return v < lo ? lo : v > hi ? hi : v;
    }

    //=======================================================================================
    struct item_cfg_t
    {
        void* userdata = nullptr;
        int parent_id = 0;

        float width = 0;
        float height = 0;

        float min_width = 0;
        float min_height = 0;

        float max_width = FLT_MAX;
        float max_height = FLT_MAX;

        int flex_grow = 0;    // Used when we need to grow
        int flex_shrink = 0;  // Used when we need to shrink
        margin_t margin;
        padding_t padding;

        // Used when the item has children
        bool horizontal = true;
        bool wrap = false;
        container_alignment_t container_alignment = container_alignment_t::start;
        container_alignment_t multi_row_alignment = container_alignment_t::start;
        item_alignment_t item_alignment = item_alignment_t::start;

        render_callback_t render_callback;
    };

    //---------------------------------------------------------------------------------------
    struct item_t
    {
        item_t(const item_cfg_t cfg, int id) : cfg(cfg), id(id)
        {
        }

        int id = -1;
        item_cfg_t cfg;

        Rectangle computed_rect;  // The usable area after layout has been performed
        std::vector<item_t*> children;
    };

    //=======================================================================================
    struct layout_t::private_t
    {
        private_t(const Rectangle& layout_rect);
        ~private_t();

        void push_config(const add_item_cfg_t& cfg);
        void pop_config();

        id add_item(const add_item_cfg_t& cfg);
        void do_layout();
        Rectangle get_rect_for_item(id item_id) const;
    

        std::vector<float> layout1d(
            container_alignment_t alignment,
            float start,
            float end,
            const std::vector<float>& items);

        void layout_container(float start_x, float start_y, const item_t* parent);

        id next_item_id_ = 1;
        std::vector<item_t*> items_;
        Rectangle layout_rect_;
        std::vector<add_item_cfg_t> config_stack_;
    };

    //=======================================================================================
    layout_t::private_t::private_t(const Rectangle& layout_rect) : layout_rect_(layout_rect)
    {
        items_.push_back(new item_t({.width = layout_rect.width, .height = layout_rect.height}, 0));
    }

    //---------------------------------------------------------------------------------------
    layout_t::private_t::~private_t()
    {
        for (item_t* item : items_) {
            delete item;
        }
        items_.clear();
    }

    //---------------------------------------------------------------------------------------
    void layout_t::private_t::push_config(const add_item_cfg_t& cfg)
    {
        // Macro that either uses the value on the config stack, or uses the supplied value, depending on if
        // `cfg.merge_config` is set or not
#define MERGE(x)                                                                                                     \
    if (cfg.merge_config && !config_stack_.empty() && config_stack_.back().x.has_value() && !merged.x.has_value()) { \
        merged.x = config_stack_.back().x;                                                                           \
    }
        add_item_cfg_t merged = cfg;
        MERGE(userdata);
        MERGE(parent_id);
        MERGE(width);
        MERGE(height);
        MERGE(min_width);
        MERGE(min_height);
        MERGE(max_width);
        MERGE(max_height);
        MERGE(flex_grow);
        MERGE(flex_shrink);
        MERGE(margin);
        MERGE(padding);
        MERGE(horizontal);
        MERGE(wrap);
        MERGE(container_alignment);
        MERGE(multi_row_alignment);
        MERGE(item_alignment);
        MERGE(render_callback);
        config_stack_.push_back(merged);
#undef MERGE
    }

    //---------------------------------------------------------------------------------------
    void layout_t::private_t::pop_config()
    {
        config_stack_.pop_back();
    }

    //---------------------------------------------------------------------------------------
    id layout_t::private_t::add_item(const add_item_cfg_t& cfg)
    {
        item_cfg_t local_cfg;

        // Macro the either used the supplied value from config, or uses the value on the config stack.
#define MERGE(x)                                                                                                      \
    if (!cfg.x.has_value() && cfg.use_config_stack && !config_stack_.empty() && config_stack_.back().x.has_value()) { \
        local_cfg.x = config_stack_.back().x.value();                                                                 \
    } else if (cfg.x.has_value()) {                                                                                   \
        local_cfg.x = cfg.x.value();                                                                                  \
    }

        MERGE(userdata);
        MERGE(parent_id);
        MERGE(width);
        MERGE(height);
        MERGE(min_width);
        MERGE(min_height);
        MERGE(max_width);
        MERGE(max_height);
        MERGE(flex_grow);
        MERGE(flex_shrink);
        MERGE(margin);
        MERGE(padding);
        MERGE(horizontal);
        MERGE(wrap);
        MERGE(container_alignment);
        MERGE(multi_row_alignment);
        MERGE(item_alignment);
        MERGE(render_callback);

#undef MERGE

        // Initialize the item
        item_t* item = new item_t(local_cfg, next_item_id_++);
        items_.push_back(item);

        // Add the item to its parent (the default is the root container)
        assert(local_cfg.parent_id >= 0);
        assert(local_cfg.parent_id < items_.size());
        items_[local_cfg.parent_id]->children.push_back(item);

        return item->id;
    }

    //---------------------------------------------------------------------------------------
    std::vector<float> layout_t::private_t::layout1d(
        container_alignment_t alignment,
        float start,
        float end,
        const std::vector<float>& items)
    {
        assert(alignment >= container_alignment_t::start);
        assert(alignment <= container_alignment_t::space_evenly);

        float total_item_size = 0;
        for (float sz : items) {
            total_item_size += sz;
        }

        const float total_layout_size = end - start;
        const float free_space = total_layout_size - total_item_size;
        assert(total_layout_size >= total_item_size);

        const size_t item_count = items.size();
        std::vector<float> positions(item_count);

        switch (alignment) {
                // see https://developer.mozilla.org/en-US/docs/Web/CSS/justify-content
            case container_alignment_t::start:
            case container_alignment_t::end:
            case container_alignment_t::center: {
                float pos;
                if (alignment == container_alignment_t::start) {
                    pos = 0;
                } else if (alignment == container_alignment_t::end) {
                    pos = start + free_space;
                } else {
                    // center
                    pos = start + free_space / 2;
                }
                for (int i = 0; i < item_count; ++i) {
                    positions[i] = pos;
                    pos += items[i];
                }
                break;
            }
            case container_alignment_t::space_between: {
                //  The items are evenly distributed within the alignment container along the main axis. The
                //  spacing between each pair of adjacent items is the same. The first item is flush with the
                //  main-start edge, and the last item is flush with the main-end edge.
                positions[0] = start;
                if (item_count > 1) {
                    float inc = free_space / (item_count - 1);
                    float pos = start + items[0] + inc;
                    for (int i = 1; i < item_count - 1; ++i) {
                        positions[i] = pos;
                        pos += items[i] + inc;
                    }
                    positions[item_count - 1] = end - items[item_count - 1];
                }
                break;
            }
            case container_alignment_t::space_around: {
                // The items are evenly distributed within the alignment container along the main axis. The
                // spacing between each pair of adjacent items is the same. The empty space before the first and
                // after the last item equals half of the space between each pair of adjacent items.
                float inc = free_space / item_count;
                float pos = start + inc / 2;
                for (int i = 0; i < item_count; ++i) {
                    positions[i] = pos;
                    pos += items[i] + inc;
                }
                break;
            }
            case container_alignment_t::space_evenly: {
                // The items are evenly distributed within the alignment container along the main axis. The
                // spacing between each pair of adjacent items, the main-start edge and the first item, and the
                // main-end edge and the last item, are all exactly the same.
                float inc = free_space / (item_count + 1);
                float pos = start + inc;
                for (int i = 0; i < item_count; ++i) {
                    positions[i] = pos;
                    pos += items[i] + inc;
                }
                break;
            }
        }

        return positions;
    }

    //---------------------------------------------------------------------------------------
    void layout_t::private_t::layout_container(float start_x, float start_y, const item_t* parent)
    {
        const bool horizontal = parent->cfg.horizontal;

        auto get_main_axis_available = [=](const item_t* item) -> float {
            // const margin_t& m = item.cfg.margin;
            //  Note, margins aren't counted as part of the item size, so they shouldn't be deducted when
            //  calculating the item size
            const padding_t& p = item->cfg.padding;
            return horizontal ? item->cfg.width - (p.left + p.right) : item->cfg.height - (p.top + p.bottom);
        };

        auto get_cross_axis_available = [=](const item_t* item) -> float {
            // const margin_t& m = item->cfg.margin;
            const padding_t& p = item->cfg.padding;
            return horizontal ? item->cfg.height - (p.top + p.bottom) : item->cfg.width - (p.left + p.right);
        };

        auto get_main_axis_size = [=](const item_t* item) -> float {
            const margin_t& m = item->cfg.margin;
            return horizontal ? item->cfg.width + (m.left + m.right) : item->cfg.height + (m.top + m.bottom);
        };

        auto get_cross_axis_size = [=](const item_t* item) -> float {
            const margin_t& m = item->cfg.margin;
            return horizontal ? item->cfg.height + (m.top + m.bottom) : item->cfg.width + (m.left + m.right);
        };

        auto get_main_axis_min_size = [=](const item_t* item) -> float {
            return horizontal ? item->cfg.min_width : item->cfg.min_height;
        };

        auto get_main_axis_max_size = [=](const item_t* item) -> float {
            return horizontal ? item->cfg.max_width : item->cfg.max_height;
        };

        auto get_cross_axis_min_size = [=](const item_t* item) -> float {
            return horizontal ? item->cfg.min_width : item->cfg.min_height;
        };

        auto get_cross_axis_max_size = [=](const item_t* item) -> float {
            return horizontal ? item->cfg.max_width : item->cfg.max_height;
        };

        const size_t child_count = parent->children.size();

        //=======================================================================================
        // Split the items into rows
        struct row_t
        {
            std::vector<item_t*> items;
            std::vector<float> item_main_axis_sizes;
            std::vector<float> item_cross_axis_sizes;
            std::vector<float> item_start;
            float main_axis_size = 0;
            float cross_axis_size = 0;
            int flex_grow_count = 0;
            int flex_shrink_count = 0;
            float total_shrink_scaled_width = 0;
        };

        std::vector<row_t> rows;
        row_t curr_row;

        float curr_row_size = 0;
        float main_axis_size = get_main_axis_available(parent);
        float cross_axis_size = get_cross_axis_available(parent);

        float cross_axis_used = 0;

        float curr_row_available = main_axis_size;

        for (int i = 0; item_t * item : parent->children) {
            float item_size = get_main_axis_size(item);
            // We clamp here to handle the case where a single item is larger than the entire row
            float clamped_item_size = flexy_min(main_axis_size, item_size);
            curr_row_available -= clamped_item_size;
            if (parent->cfg.wrap && curr_row_available < 0) {
                // No space left on the current row, so store it
                rows.push_back(curr_row);
                cross_axis_used += curr_row.cross_axis_size;
                curr_row = row_t{};
                curr_row_available = main_axis_size;
            }

            curr_row.items.push_back(item);
            curr_row.item_main_axis_sizes.push_back(item_size);
            curr_row.main_axis_size += item_size;
            float cross_axis_size =
                flexy_clamp(get_cross_axis_size(item), get_cross_axis_min_size(item), get_cross_axis_max_size(item));
            curr_row.item_cross_axis_sizes.push_back(cross_axis_size);
            curr_row.cross_axis_size = flexy_max(curr_row.cross_axis_size, cross_axis_size);
            curr_row.flex_grow_count += item->cfg.flex_grow;
            curr_row.flex_shrink_count += item->cfg.flex_shrink;
            curr_row.total_shrink_scaled_width += item->cfg.flex_shrink * item_size;
            ++i;
        }

        if (!curr_row.items.empty()) {
            rows.push_back(curr_row);
            cross_axis_used += curr_row.cross_axis_size;
        }

        //=======================================================================================
        // Calculate the item sizes (using growth/shrink rules)
        for (row_t& row : rows) {
            if (row.main_axis_size > main_axis_size && row.flex_shrink_count > 0) {
                // If flex_shrink_count is 0, then we don't resize any items
                float delta = row.main_axis_size - main_axis_size;
                float new_row_size = 0;
                for (int i = 0; item_t * item : row.items) {
                    // Reduce each item's size depending on the shrink_value
                    // float s1 = float(item->cfg.flex_shrink) / row.flex_shrink_count;
                    // Calculation from https://www.samanthaming.com/flexbox30/24-flex-shrink-calculation/
                    float ratio = get_main_axis_size(item) * item->cfg.flex_shrink / row.total_shrink_scaled_width;
                    float sz = flexy_max(get_main_axis_min_size(item), get_main_axis_size(item) - ratio * delta);
                    row.item_main_axis_sizes[i] = sz;
                    new_row_size += sz;
                    ++i;
                }
                row.main_axis_size = new_row_size;
            }

            if (row.main_axis_size < main_axis_size && row.flex_grow_count > 0) {
                // There is some free space available, so resize
                float delta = main_axis_size - row.main_axis_size;
                float new_row_size = 0;
                for (int i = 0; item_t * item : row.items) {
                    // Increase each item's size depending on the shrink_value
                    float sz = flexy_min(
                        get_main_axis_max_size(item),
                        get_main_axis_size(item) + delta * item->cfg.flex_grow / row.flex_grow_count);
                    row.item_main_axis_sizes[i] = sz;
                    new_row_size += sz;
                    ++i;
                }
                row.main_axis_size = new_row_size;
            }
        }

        //=======================================================================================
        // Lay out the items

        // justify-items: align items within a row along the main axis
        // align-content: align rows along the cross axis
        // align-items: align items within a row along the cross axis

        // Lay out the items in each row along the main axis
        for (row_t& row : rows) {
            if (row.main_axis_size < main_axis_size) {
                row.item_start = layout1d(parent->cfg.container_alignment, 0, main_axis_size, row.item_main_axis_sizes);
            } else {
                // The row is full, so just lay the items out one after another
                float pos = 0;
                for (int i = 0; item_t * item : row.items) {
                    row.item_start.push_back(pos);
                    pos += row.item_main_axis_sizes[i];
                    ++i;
                }
            }
        }

        std::vector<float> row_sizes;
        std::vector<float> row_start;

        // Lay out the rows along the cross axis
        if (cross_axis_size > cross_axis_used) {
            // We have cross axis space available, so lay out the rows

            if (parent->cfg.multi_row_alignment == container_alignment_t::stretch) {
                // I'm doing my own version of stretch here, where I add the remaining space equally over the rows
                float delta = (cross_axis_size - cross_axis_used) / rows.size();
                float start = 0;
                for (const row_t& row : rows) {
                    row_sizes.push_back(row.cross_axis_size + delta);
                    row_start.push_back(start);
                    start += row_sizes.back();
                }
            } else {
                for (const row_t& row : rows) {
                    row_sizes.push_back(row.cross_axis_size);
                }
                row_start =
                    layout1d((container_alignment_t)parent->cfg.multi_row_alignment, 0, cross_axis_size, row_sizes);
            }

        } else {
            // There is no additional space, so just lay the rows out one after another
            float start = 0;
            for (const row_t& row : rows) {
                row_sizes.push_back(row.cross_axis_size);
                row_start.push_back(start);
                start += row_sizes.back();
            }
        }

        // Now we can lay out the actual items in each row
        for (int i = 0; row_t & row : rows) {
            float curr_row_start = row_start[i];
            float curr_row_size = row_sizes[i];

            for (int j = 0; item_t * item : row.items) {
                float cross_start, cross_end;
                switch (parent->cfg.item_alignment) {
                    case item_alignment_t::start: {
                        cross_start = curr_row_start;
                        cross_end = cross_start + row.item_cross_axis_sizes[j];
                        break;
                    }
                    case item_alignment_t::end: {
                        cross_start = curr_row_start + curr_row_size - row.item_cross_axis_sizes[j];
                        cross_end = cross_start + row.item_cross_axis_sizes[j];
                        break;
                    }
                    case item_alignment_t::center: {
                        cross_start = curr_row_start + (curr_row_size - row.item_cross_axis_sizes[j]) / 2;
                        cross_end = cross_start + row.item_cross_axis_sizes[j];
                        break;
                    }
                    case item_alignment_t::stretch: {
                        cross_start = curr_row_start;
                        cross_end = curr_row_start + curr_row_size;
                        break;
                    }
                }

                const margin_t& m = item->cfg.margin;
                const padding_t& p = item->cfg.padding;

                // Create the rectangle for the data we have, and invoke the render callback
                if (horizontal) {
                    item->computed_rect = {
                        start_x + row.item_start[j] + m.left + p.left,
                        start_y + cross_start + m.top + p.top,
                        row.item_main_axis_sizes[j] - (m.left + m.right + p.left + p.right),
                        cross_end - cross_start - (m.top + m.bottom + p.top + p.bottom),
                    };
                } else {
                    item->computed_rect = {
                        start_x + cross_start + m.left + p.left,
                        start_y + row.item_start[j] + m.top + p.top,
                        cross_end - cross_start - (m.left + m.right + p.left + p.right),
                        row.item_main_axis_sizes[j] - (m.top + m.bottom + p.top + p.bottom),
                    };
                }

                if (item->cfg.render_callback) {
                    item->cfg.render_callback(item->cfg.userdata, item->computed_rect);
                }

                layout_container(item->computed_rect.x, item->computed_rect.y, item);
                ++j;
            }
            ++i;
        }
    }

    //---------------------------------------------------------------------------------------
    void layout_t::private_t::do_layout()
    {
        // Add a scissor rect to disallow drawing outside the main layout
        BeginScissorMode((int)layout_rect_.x, (int)layout_rect_.y, (int)layout_rect_.width, (int)layout_rect_.height);
        layout_container(layout_rect_.x, layout_rect_.y, items_[0]);
        EndScissorMode();
    }

    //---------------------------------------------------------------------------------------
    Rectangle layout_t::private_t::get_rect_for_item(id item_id) const
    {
        assert(item_id >= 0);
        assert(item_id < (int)items_.size());

        if (item_id >= 0 && item_id < (int)items_.size()) {
            return items_[item_id]->computed_rect;
        }
        return Rectangle{0, 0, 0, 0};
    }

    //=======================================================================================
    layout_t::layout_t(const Rectangle& layout_rect) : p_(new private_t(layout_rect))
    {
    }

    //---------------------------------------------------------------------------------------
    layout_t::~layout_t()
    {
        delete p_;
    }

    //---------------------------------------------------------------------------------------
    void layout_t::push_config(const add_item_cfg_t& cfg)
    {
        p_->push_config(cfg);
    }

    //---------------------------------------------------------------------------------------
    void layout_t::pop_config()
    {
        p_->pop_config();
    }

    //---------------------------------------------------------------------------------------
    id layout_t::add_item(const add_item_cfg_t& cfg)
    {
        return p_->add_item(cfg);
    }

    //---------------------------------------------------------------------------------------
    void layout_t::do_layout()
    {
        p_->do_layout();
    }

    //---------------------------------------------------------------------------------------
    Rectangle layout_t::get_rect_for_item(id item_id) const
    {
        return p_->get_rect_for_item(item_id);
    }

    //=======================================================================================
    scoped_config_t::scoped_config_t(const add_item_cfg_t& cfg, layout_t* layout) : layout(layout)
    {
        layout->push_config(cfg);
    }

    //---------------------------------------------------------------------------------------
    scoped_config_t::~scoped_config_t()
    {
        layout->pop_config();
    }

}  // namespace flexy
