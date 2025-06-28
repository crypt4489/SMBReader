#pragma once
#include <cstdint>
class ImageIndex
{
public:
	ImageIndex() = default;
	explicit ImageIndex(uint16_t _id) : ID(_id) {};
	explicit ImageIndex(uint32_t _id) : ID(static_cast<uint16_t>(_id)) {};
	explicit ImageIndex(size_t _id) : ID(static_cast<uint16_t>(_id)) {};
	//delete copies / integer assignment
	constexpr ImageIndex& operator=(const uint32_t) = delete;
	constexpr ImageIndex& operator=(const ImageIndex&) = delete;
	ImageIndex(const ImageIndex& other) {
		this->ID = ~0; //invalidate, needed only to compile on msvc
	}

	//keep moves
	ImageIndex(ImageIndex&&) = default;
	constexpr ImageIndex& operator=(ImageIndex&&) = default;

	constexpr auto operator()() const {
		return ID;
	}

	constexpr operator size_t() const {
		return ID;
	}

	constexpr operator uint32_t() const {
		return ID;
	}

	constexpr bool operator==(const ImageIndex& other) const {
		return this->ID == other.ID;
	}
private:
	uint16_t ID = ~0;
};


class BufferIndex
{
public:
	BufferIndex() = default;
	explicit BufferIndex(uint16_t _id) : ID(_id) {};
	explicit BufferIndex(uint32_t _id) : ID(static_cast<uint16_t>(_id)) {};
	explicit BufferIndex(size_t _id) : ID(static_cast<uint16_t>(_id)) {};
	//delete copies / integer assignment
	constexpr BufferIndex& operator=(const uint32_t) = delete;
	constexpr BufferIndex& operator=(const BufferIndex&) = delete;
	BufferIndex(const BufferIndex& other) {
		this->ID = ~0; //invalidate, needed only to compile on msvc
	}

	//keep moves
	BufferIndex(BufferIndex&&) = default;
	constexpr BufferIndex& operator=(BufferIndex&&) = default;

	constexpr auto operator()() const {
		return ID;
	}

	constexpr operator size_t() const {
		return ID;
	}

	constexpr operator uint32_t() const {
		return ID;
	}

	constexpr bool operator==(const BufferIndex& other) const {
		return this->ID == other.ID;
	}
private:
	uint16_t ID = ~0;
};

class DeviceIndex
{
public:
	DeviceIndex() = default;
	explicit DeviceIndex(uint8_t _id) : ID(_id) {};
	explicit DeviceIndex(uint32_t _id) : ID(static_cast<uint8_t>(_id)) {};
	explicit DeviceIndex(size_t _id) : ID(static_cast<uint8_t>(_id)) {};
	//delete copies / integer assignment
	constexpr DeviceIndex& operator=(const uint32_t) = delete;
	constexpr DeviceIndex& operator=(const DeviceIndex&) = delete;
	DeviceIndex(const DeviceIndex& other) = delete;

	//keep moves
	DeviceIndex(DeviceIndex&&) = default;
	constexpr DeviceIndex& operator=(DeviceIndex&&) = default;

	constexpr auto operator()() const {
		return ID;
	}

	constexpr operator size_t() const {
		return ID;
	}

	constexpr operator uint32_t() const {
		return ID;
	}

	constexpr bool operator==(const DeviceIndex& other) const {
		return this->ID == other.ID;
	}
private:
	uint8_t ID = ~0;
};