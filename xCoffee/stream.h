#pragma once

#include <string>
#include <string_view>
#include <memory>
#include <mutex>

namespace xCoffee
{
	class TStreamError
	{
	public:
		enum class EErrorId : uint32_t
		{
			__NO_ERROR = 0,
			FILE_NO_FOUND,
			FILE_CANT_OPEN,
			FILE_CANT_CREATE,
			FILE_IO_ACCESS,
			STREAM_EMPTY,
			OUT_OF_MEMORY,
			INVALID_ARGUMENTS,
			INTERNAL_EXCEPTION,
			__MAX,
		};
	protected:
		EErrorId errorId{ EErrorId::__NO_ERROR };

		inline void ResetError() noexcept { errorId = EErrorId::__NO_ERROR; }
		inline void ResetError() const noexcept { const_cast<TStreamError*>(this)->errorId = EErrorId::__NO_ERROR; }
		inline void SetErrorId(EErrorId a_errorId) noexcept { errorId = a_errorId; }
		inline void SetErrorId(EErrorId a_errorId) const noexcept { const_cast<TStreamError*>(this)->errorId = a_errorId; }
		[[nodiscard]] inline bool SetErrorIdAndReturn(EErrorId a_errorId) noexcept { SetErrorId(a_errorId); return false; }
		[[nodiscard]] inline bool SetErrorIdAndReturn(EErrorId a_errorId) const noexcept { SetErrorId(a_errorId); return false; }
	public:
		constexpr TStreamError() = default;
		virtual ~TStreamError() = default;

		inline TStreamError(TStreamError&& a_rhs) noexcept : errorId(a_rhs.errorId) {};
		inline TStreamError(const TStreamError& a_rhs) noexcept : errorId(a_rhs.errorId) {};
		[[nodiscard]] inline TStreamError& operator=(TStreamError&& a_rhs) noexcept { errorId = a_rhs.errorId; };
		[[nodiscard]] inline TStreamError& operator=(const TStreamError& a_rhs) noexcept { errorId = a_rhs.errorId; };

		[[nodiscard]] inline virtual EErrorId GetErrorId() const noexcept { return errorId; }
		[[nodiscard]] virtual std::string_view GetErrorAsString() const noexcept;
	};

	class TStreamLocker
	{
		std::recursive_mutex locker{};
	public:
		constexpr TStreamLocker() = default;
		virtual ~TStreamLocker() = default;

		virtual void Lock() noexcept;
		virtual void Unlock() noexcept;
		[[nodiscard]] virtual bool TryLock() noexcept;
	};

	class TStreamGuard
	{
		TStreamLocker* locker{};

		TStreamGuard(TStreamGuard&& a_rhs) = delete;
		TStreamGuard(const TStreamGuard& a_rhs) = delete;
		TStreamGuard& operator=(TStreamGuard&& a_rhs) = delete;
		TStreamGuard& operator=(const TStreamGuard& a_rhs) = delete;
	public:
		inline TStreamGuard(TStreamLocker& a_locker) : locker(&a_locker) { locker->Lock(); };
		inline ~TStreamGuard() { locker->Unlock(); };
	};

	class IStream :
		public TStreamError
	{
	protected:
		TStreamLocker locker;
	public:
		enum class EDirect
		{
			BEGIN = 0,
			CURRENT,
			END,
		};

		constexpr IStream() = default;
		virtual ~IStream() = default;

		IStream(IStream&& a_rhs) noexcept;
		IStream(const IStream& a_rhs) noexcept;
		IStream& operator=(IStream&& a_rhs) noexcept;
		IStream& operator=(const IStream& a_rhs) noexcept;

		[[nodiscard]] virtual uint32_t Read(void* a_buffer, uint32_t a_size) noexcept = 0;
		[[nodiscard]] virtual uint32_t Write(const void* a_buffer, uint32_t a_size) noexcept = 0;
		[[nodiscard]] virtual bool ReadBuffer(void* a_buffer, uint32_t a_size) noexcept;
		[[nodiscard]] virtual bool WriteBuffer(const void* a_buffer, uint32_t a_size) noexcept;
		[[nodiscard]] virtual uint32_t Seek(int32_t a_offset, EDirect a_direct = EDirect::CURRENT) noexcept = 0;
		[[nodiscard]] virtual uint32_t CopyFrom(IStream& a_from, uint32_t a_size = 0) noexcept;
		[[nodiscard]] virtual uint32_t Size() const noexcept;
		[[nodiscard]] inline virtual bool Empty() const noexcept { return !Size(); };
		[[nodiscard]] virtual uint32_t GetPosition() const noexcept;
		[[nodiscard]] virtual void SetPosition(uint32_t a_newPosition) noexcept;
	};

	class TFileStream :
		public IStream
	{
	protected:
		void* handle{ nullptr };
	public:
		enum class EMode
		{
			CREATE = 0,
			OPEN_READ,
			OPEN_READWRITE,
		};

		constexpr TFileStream() = default;
		TFileStream(const char* a_filename, EMode a_mode) noexcept;
		TFileStream(const std::string& a_filename, EMode a_mode) noexcept;
		TFileStream(const std::string_view& a_filename, EMode a_mode) noexcept;
		virtual ~TFileStream();

		TFileStream(TFileStream&& a_rhs) noexcept;
		TFileStream(const TFileStream& a_rhs) noexcept;
		TFileStream& operator=(TFileStream&& a_rhs) noexcept;
		TFileStream& operator=(const TFileStream& a_rhs) noexcept;

		[[nodiscard]] virtual bool Open(const char* a_filename, EMode a_mode) noexcept;
		[[nodiscard]] virtual bool Open(const std::string& a_filename, EMode a_mode) noexcept;
		[[nodiscard]] virtual bool Open(const std::string_view& a_filename, EMode a_mode) noexcept;

		[[nodiscard]] inline virtual bool IsOpen() const noexcept { return handle != nullptr; }
		[[nodiscard]] virtual bool IsGood() const noexcept;
		[[nodiscard]] virtual bool IsBad() const noexcept;

		virtual void Close() noexcept;
		virtual void Flush() noexcept;

		[[nodiscard]] uint32_t Read(void* a_buffer, uint32_t a_size) noexcept override;
		[[nodiscard]] uint32_t Write(const void* a_buffer, uint32_t a_size) noexcept override;
		[[nodiscard]] uint32_t Seek(int32_t a_offset, EDirect a_direct = EDirect::CURRENT) noexcept override;
	};

	class IFileStreamOp
	{
	public:
		[[nodiscard]] virtual bool LoadFromStream(IStream& a_stream) noexcept = 0;
		[[nodiscard]] virtual bool LoadFromFile(const char* a_filename) noexcept;
		[[nodiscard]] virtual bool LoadFromFile(const std::string& a_filename) noexcept;
		[[nodiscard]] virtual bool LoadFromFile(const std::string_view& a_filename) noexcept;

		[[nodiscard]] virtual bool SaveToStream(IStream& a_stream) const noexcept = 0;
		[[nodiscard]] virtual bool SaveToFile(const char* a_filename) const noexcept;
		[[nodiscard]] virtual bool SaveToFile(const std::string& a_filename) const noexcept;
		[[nodiscard]] virtual bool SaveToFile(const std::string_view& a_filename) const noexcept;
	};

	class TBinaryStream :
		public IStream,
		public IFileStreamOp
	{
		uint32_t size{ 0 };
		uint32_t position{ 0 };
		std::unique_ptr<uint8_t[]> memory{};
	public:
		constexpr TBinaryStream() = default;
		TBinaryStream(const void* a_buffer, uint32_t a_size) noexcept;
		virtual ~TBinaryStream() = default;

		TBinaryStream(TBinaryStream&& a_rhs) noexcept;
		TBinaryStream(const TBinaryStream& a_rhs) noexcept;
		TBinaryStream& operator=(TBinaryStream&& a_rhs) noexcept;
		TBinaryStream& operator=(const TBinaryStream& a_rhs) noexcept;

		[[nodiscard]] virtual const uint8_t* Memory() const noexcept { return memory.get(); }
		[[nodiscard]] virtual uint8_t* Memory() noexcept { return memory.get(); }
		[[nodiscard]] virtual bool Resize(uint32_t a_size) noexcept;

		inline virtual void Clear() noexcept { memory.reset(); size = 0; }

		[[nodiscard]] uint32_t Read(void* a_buffer, uint32_t a_size) noexcept override;
		[[nodiscard]] uint32_t Write(const void* a_buffer, uint32_t a_size) noexcept override;
		[[nodiscard]] uint32_t Seek(int32_t a_offset, EDirect a_direct = EDirect::CURRENT) noexcept override;
		[[nodiscard]] inline uint32_t Size() const noexcept override { return size; }
		[[nodiscard]] uint32_t GetPosition() const noexcept override { return position; }

		[[nodiscard]] bool LoadFromStream(IStream& a_stream) noexcept override;
		[[nodiscard]] bool SaveToStream(IStream& a_stream) const noexcept override;
	};

	// Only Utf8 or Ansi codepages
	class TTextFileStream :
		public TFileStream
	{
	protected:
		void WriteLineStr(const std::string& string) const noexcept;
	public:
		[[nodiscard]] bool Open(const char* a_filename, EMode a_mode) noexcept override;
		[[nodiscard]] bool Open(const std::string& a_filename, EMode a_mode) noexcept override;
		[[nodiscard]] bool Open(const std::string_view& a_filename, EMode a_mode) noexcept override;

		[[nodiscard]] bool IsGood() const noexcept override;
		[[nodiscard]] bool IsBad() const noexcept override;

		void Close() noexcept override;
		void Flush() noexcept override;

		[[nodiscard]] uint32_t Read(void* a_buffer, uint32_t a_size) noexcept override;
		[[nodiscard]] uint32_t Write(const void* a_buffer, uint32_t a_size) noexcept override;
		[[nodiscard]] uint32_t Seek(int32_t a_offset, EDirect a_direct = EDirect::CURRENT) noexcept override;

		[[nodiscard]] virtual bool ReadLine(char* string, std::uint32_t maxsize) const noexcept;
		virtual void WriteString(const std::string_view& formatted_string, ...) const noexcept;
		virtual void WriteString(const std::wstring_view& formatted_string, ...) const noexcept;
		virtual void WriteString(const char* formatted_string, ...) const noexcept;
		virtual void WriteString(const wchar_t* formatted_string, ...) const noexcept;
		virtual void WriteLine(const std::string_view& formatted_string, ...) const noexcept;
		virtual void WriteLine(const std::wstring_view& formatted_string, ...) const noexcept;
		virtual void WriteLine(const char* formatted_string, ...) const noexcept;
		virtual void WriteLine(const wchar_t* formatted_string, ...) const noexcept;
	public:
		constexpr TTextFileStream() = default;
		TTextFileStream(const char* a_filename, EMode a_mode) noexcept;
		TTextFileStream(const std::string& a_filename, EMode a_mode) noexcept;
		TTextFileStream(const std::string_view& a_filename, EMode a_mode) noexcept;
	};
};