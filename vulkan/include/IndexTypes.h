#pragma once
#include <cstdint>

struct DeviceIndex
{

	DeviceIndex() = default;
	explicit DeviceIndex(uint8_t _id) : ID(_id) {};
	explicit DeviceIndex(uint32_t _id) : ID(static_cast<uint8_t>(_id)) {};
	explicit DeviceIndex(size_t _id) : ID(static_cast<uint8_t>(_id)) {};

	constexpr DeviceIndex& operator=(const DeviceIndex&) = default;
	DeviceIndex(const DeviceIndex& other) = default;

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

	uint8_t ID = ~0ui8;
};

struct QueueIndex
{

	QueueIndex() = default;
	explicit QueueIndex(uint8_t _id) : ID(_id) {};
	explicit QueueIndex(uint32_t _id) : ID(static_cast<uint8_t>(_id)) {};
	explicit QueueIndex(size_t _id) : ID(static_cast<uint8_t>(_id)) {};
	
	constexpr QueueIndex& operator=(const uint32_t i) {
		this->ID = static_cast<uint8_t>(i);
		return *this;
	};
	constexpr QueueIndex& operator=(const QueueIndex&) = default;
	QueueIndex(const QueueIndex& other) = default;

	//keep moves
	QueueIndex(QueueIndex&&) = default;
	constexpr QueueIndex& operator=(QueueIndex&&) = default;

	constexpr auto operator()() const {
		return ID;
	}

	constexpr operator size_t() const {
		return ID;
	}

	constexpr operator uint32_t() const {
		return ID;
	}

	constexpr bool operator==(const QueueIndex& other) const {
		return this->ID == other.ID;
	}

	

	uint8_t ID = ~0ui8;
};


struct EntryHandle
{

	EntryHandle() = default;
	~EntryHandle() = default;

	explicit EntryHandle(size_t _id) : ID(_id) {};

	
	EntryHandle& operator=(const size_t n) {
		this->ID = n;
		return *this;
	};
	
	constexpr EntryHandle& operator=(const EntryHandle& other) {
		this->ID = other.ID;
		return *this;
	};

	EntryHandle(const EntryHandle& other)
	{
		this->ID = other.ID;
	}

	//keep moves
	EntryHandle(EntryHandle&&) = default;
	constexpr EntryHandle& operator=(EntryHandle&&) = default;

	constexpr auto operator()() const {
		return ID;
	}

	constexpr operator size_t() const {
		return ID;
	}

	constexpr operator uint32_t() const {
		return static_cast<uint32_t>(ID);
	}

	EntryHandle operator+(const EntryHandle& other)
	{
		return EntryHandle(other.ID + ID);
	}

	EntryHandle operator+(const size_t other)
	{
		return EntryHandle(other + ID);
	}

	EntryHandle operator+(const int other)
	{
		return EntryHandle(other + ID);
	}

	EntryHandle operator+(const uint32_t other)
	{
		return EntryHandle(other + ID);
	}

	constexpr bool operator==(const EntryHandle& other) const {
		return this->ID == other.ID;
	}

	constexpr bool operator==(size_t n) const {
		return this->ID == n;
	}

	constexpr bool operator!=(size_t n) const {
		return this->ID != n;
	}

	size_t ID = ~0ui64;
};


