#pragma once

#include <array>
#include <atomic>
#include <cstddef>

namespace jp_audio_internal
{
	template <std::size_t BlockFrames, std::size_t Capacity>
	class SpscAudioQueue
	{
	public:
		struct Block
		{
			std::array<float, BlockFrames> samples{};
			std::size_t count = 0;
		};

		bool push(const float *samples, std::size_t count)
		{
			const unsigned int write = write_.load(std::memory_order_relaxed);
			const unsigned int read = read_.load(std::memory_order_acquire);
			if (write - read >= Capacity)
			{
				dropped_.fetch_add(1, std::memory_order_relaxed);
				return false;
			}
			Block &slot = blocks_[write % Capacity];
			slot.count = count < BlockFrames ? count : BlockFrames;
			for (std::size_t i = 0; i < slot.count; ++i) slot.samples[i] = samples[i];
			write_.store(write + 1, std::memory_order_release);
			return true;
		}

		bool pop(Block &result)
		{
			const unsigned int read = read_.load(std::memory_order_relaxed);
			if (read == write_.load(std::memory_order_acquire)) return false;
			result = blocks_[read % Capacity];
			read_.store(read + 1, std::memory_order_release);
			return true;
		}

		// Only call while the producer is stopped.
		void reset()
		{
			const unsigned int write = write_.load(std::memory_order_acquire);
			read_.store(write, std::memory_order_release);
			dropped_.store(0, std::memory_order_relaxed);
		}

		unsigned long long dropped() const
		{
			return dropped_.load(std::memory_order_relaxed);
		}

	private:
		std::array<Block, Capacity> blocks_{};
		std::atomic<unsigned int> write_{0};
		std::atomic<unsigned int> read_{0};
		std::atomic<unsigned long long> dropped_{0};
	};
}
