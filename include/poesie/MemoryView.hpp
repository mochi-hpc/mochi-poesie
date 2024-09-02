/*
 * (C) 2024 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#ifndef __POESIE_MEMORY_VIEW_HPP
#define __POESIE_MEMORY_VIEW_HPP

#include <thallium.hpp>
#include <nlohmann/json.hpp>
#include <optional>
#include <memory>
#include <cstdint>

namespace poesie {

class MemoryViewImpl;

class MemoryView {

    public:

    enum class Intent : std::uint8_t {
        IN    = 0x1,
        OUT   = 0x2,
        INOUT = 0x3
    };

    /**
     * @brief Default constructor.
     */
    MemoryView();

    /**
     * @brief Constructor using a local pointer and size.
     */
    MemoryView(thallium::engine engine,
               const char* data,
               size_t size,
               Intent intent);

    /**
     * @brief Constructor using a serialized JSON object.
     */
    MemoryView(thallium::engine engine,
               const nlohmann::json& j);

    /**
     * @brief Constructore for a remote memory view.
     *
     * @param engine Engine.
     * @param remote_bulk Bulk handle.
     * @param remote_owner Address of owner.
     * @param intent Intent of the buffer.
     * @param offset Offset in the bulk (default 0).
     * @param size Size of the bulk (default = size of the bulk - offset).
     */
    MemoryView(thallium::engine engine,
               thallium::bulk remote_bulk,
               thallium::endpoint remote_owner,
               Intent intent,
               std::optional<size_t> remote_offset = std::nullopt,
               std::optional<size_t> remote_size = std::nullopt);

    /**
     * @brief Copy constructor.
     */
    MemoryView(const MemoryView&);

    /**
     * @brief Move constructor.
     */
    MemoryView(MemoryView&&);

    /**
     * @brief Copy-assignment operator.
     */
    MemoryView& operator=(const MemoryView&);

    /**
     * @brief Move-assignment operator.
     */
    MemoryView& operator=(MemoryView&&);

    /**
     * @brief Destructor.
     */
    ~MemoryView();

    /**
     * @brief Return pointer to local data.
     */
    char* data() const;

    /**
     * @brief Return size of the data.
     */
    size_t size() const;

    /**
     * @brief Get the intent of the MemoryView.
     */
    Intent intent() const;

    /**
     * @brief Checks that the MemoryView instance is valid.
     */
    operator bool() const;

    /**
     * @brief Check for equality.
     */
    bool operator==(const MemoryView& other) const;

    /**
     * @brief Convert the MemoryView into JSON.
     */
    nlohmann::json toJson() const;

    /**
     * @brief Checks if the JSON object is convertible to a MemoryView.
     */
    static bool IsMemoryView(const nlohmann::json& j);

    private:

    std::shared_ptr<MemoryViewImpl> self;
};

static inline void to_json(nlohmann::json& j, const MemoryView& m) {
    j = m.toJson();
}

}

#endif
