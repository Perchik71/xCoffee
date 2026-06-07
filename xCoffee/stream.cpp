#include <array>
#include <memory>
#include <fstream>
#include <filesystem>
#include <cstdio>
#include <cstdarg>

#include "stream.h"
#include "util.h"

namespace xCoffee
{
	using namespace std::literals;

    static bool __TryCopyMemorySEH(void* a_dest, const void* a_source, size_t a_size) noexcept
    {
        if (!a_dest || !a_source)
            return false;

        if (!a_size)
            return true;

        __try
        {
            memcpy(a_dest, a_source, a_size);
            return true;
        }
        __except (1)
        {
            return false;
        }
    }

	static std::array<std::string_view, std::to_underlying(TStreamError::EErrorId::__MAX)> g_StreamError{{
            "No error"sv,
            "File no found"sv,
            "File can't open"sv,
            "File can't create"sv,
            "File I/O access error"sv,
            "Stream is empty"sv,
            "Out of memory"sv,
            "Invalid arguments"sv,
            "Internal exception"sv,
	}};

	std::string_view TStreamError::GetErrorAsString() const noexcept
	{
		return g_StreamError[std::to_underlying(errorId)];
	}

    // ==================== TStreamLocker ===================== //

    void TStreamLocker::Lock() noexcept
    {
        locker.lock();
    }

    void TStreamLocker::Unlock() noexcept
    {
        locker.unlock();
    }

    bool TStreamLocker::TryLock() noexcept
    {
        return locker.try_lock();
    }

    // ======================== IStream ======================== //

    IStream::IStream(IStream&& a_rhs) noexcept
    {
        *this = a_rhs;
    }

    IStream::IStream(const IStream& a_rhs) noexcept
    {
        *this = a_rhs;
    }

    IStream& IStream::operator=(IStream&& a_rhs) noexcept
    {
        (void)CopyFrom(a_rhs);
        return *this;
    }

    IStream& IStream::operator=(const IStream& a_rhs) noexcept
    {
        (void)CopyFrom(const_cast<IStream&>(a_rhs));
        return *this;
    }

    bool IStream::ReadBuffer(void* a_buffer, uint32_t a_size) noexcept
    {
        TStreamGuard guard(locker);

        ResetError();
        if (Read(a_buffer, a_size) != a_size)
            return SetErrorIdAndReturn(EErrorId::FILE_IO_ACCESS);
        return true;
    }

    bool IStream::WriteBuffer(const void* a_buffer, uint32_t a_size) noexcept
    {
        TStreamGuard guard(locker);

        ResetError();
        auto tt = Write(a_buffer, a_size);
        if (tt != a_size)
            return SetErrorIdAndReturn(EErrorId::FILE_IO_ACCESS);
        return true;
    }

    uint32_t IStream::CopyFrom(IStream& a_from, uint32_t a_size) noexcept
    {
        TStreamGuard guard(locker);

        ResetError();

        if (a_from.Empty())
            return (uint32_t)SetErrorIdAndReturn(EErrorId::STREAM_EMPTY);

        uint32_t size = a_size;
        if (!size)
        {
            size = a_from.Size();
            (void)Seek(0, EDirect::BEGIN);
        }

        constexpr static auto READ_SIZE = 64u * 1024;

        uint32_t read = READ_SIZE;
        uint32_t count = 0;
        auto buffer = std::make_unique<uint8_t[]>(READ_SIZE);
        if (!buffer)
            return (uint32_t)SetErrorIdAndReturn(EErrorId::OUT_OF_MEMORY);

        do
        {
            if (read > (size - count))
                read = size - count;

            if (!a_from.ReadBuffer(buffer.get(), read))
                return count;

            if (!WriteBuffer(buffer.get(), read))
                return count;

            count += read;
        } while (size - count);

        return count;
    }

    uint32_t IStream::Size() const noexcept
    {
        auto __this = const_cast<IStream*>(this);
        TStreamGuard guard(__this->locker);
        auto safe = __this->Seek(0);
        auto size = __this->Seek(0, EDirect::END);
        (void)__this->Seek(safe, EDirect::BEGIN);
        return size;
    }

    uint32_t IStream::GetPosition() const noexcept
    {
        auto __this = const_cast<IStream*>(this);
        return __this->Seek(0);
    }

    void IStream::SetPosition(uint32_t a_newPosition) noexcept
    {
        (void)Seek(a_newPosition, EDirect::BEGIN);
    }

    // ====================== TFileStream ====================== //

    TFileStream::TFileStream(const char* a_filename, EMode a_mode) noexcept
    {
        (void)Open(a_filename, a_mode);
    }

    TFileStream::TFileStream(const std::string& a_filename, EMode a_mode) noexcept
    {
        (void)Open(a_filename, a_mode);
    }

    TFileStream::TFileStream(const std::string_view& a_filename, EMode a_mode) noexcept
    {
        (void)Open(a_filename, a_mode);
    }

    TFileStream::~TFileStream()
    {
        Close();
    }

    TFileStream::TFileStream(TFileStream&& a_rhs) noexcept
    {
        *this = a_rhs;
    }

    TFileStream::TFileStream(const TFileStream& a_rhs) noexcept
    {
        *this = a_rhs;
    }

    TFileStream& TFileStream::operator=(TFileStream&& a_rhs) noexcept
    {
        (void)CopyFrom(a_rhs);
        return *this;
    }

    TFileStream& TFileStream::operator=(const TFileStream& a_rhs) noexcept
    {
        (void)CopyFrom(const_cast<TFileStream&>(a_rhs));
        return *this;
    }

    bool TFileStream::Open(const char* a_filename, EMode a_mode) noexcept
    {
        return Open(std::string(a_filename), a_mode);
    }

    bool TFileStream::Open(const std::string& a_filename, EMode a_mode) noexcept
    {
        if (IsOpen())
            Close();

        std::filesystem::path filename(TConvertUtil::ToDecode(a_filename));

        if (std::filesystem::is_directory(filename))
            return SetErrorIdAndReturn(EErrorId::INVALID_ARGUMENTS);

        if ((a_mode != EMode::CREATE) && (!std::filesystem::exists(filename)))
            return SetErrorIdAndReturn(EErrorId::FILE_NO_FOUND);

        if (a_mode == EMode::CREATE)
        {
            if (std::filesystem::exists(filename) && !std::filesystem::remove(filename))
                return SetErrorIdAndReturn(EErrorId::FILE_IO_ACCESS);
        }

        handle = new std::fstream(filename, (a_mode == EMode::OPEN_READ ? std::ios::in : std::ios::out) |
            std::ios::binary);
        if (!handle)
            return SetErrorIdAndReturn(a_mode == EMode::CREATE ? EErrorId::FILE_CANT_CREATE : EErrorId::FILE_CANT_OPEN);

        if (!((std::fstream*)handle)->is_open())
        {
            Close();
            return SetErrorIdAndReturn(EErrorId::FILE_IO_ACCESS);
        }

        return true;
    }

    bool TFileStream::Open(const std::string_view& a_filename, EMode a_mode) noexcept
    {
        if (IsOpen())
            Close();

        std::filesystem::path filename(TConvertUtil::ToDecode(a_filename));

        if (std::filesystem::is_directory(filename))
            return SetErrorIdAndReturn(EErrorId::INVALID_ARGUMENTS);

        if ((a_mode != EMode::CREATE) && (!std::filesystem::exists(filename)))
            return SetErrorIdAndReturn(EErrorId::FILE_NO_FOUND);

        if (a_mode == EMode::CREATE)
        {
            if (std::filesystem::exists(filename) && !std::filesystem::remove(filename))
                return SetErrorIdAndReturn(EErrorId::FILE_IO_ACCESS);
        }

        handle = new std::fstream(filename, (a_mode == EMode::OPEN_READ ? std::ios::in : std::ios::out) |
            std::ios::binary);
        if (!handle)
            return SetErrorIdAndReturn(a_mode == EMode::CREATE ? EErrorId::FILE_CANT_CREATE : EErrorId::FILE_CANT_OPEN);

        if (!((std::fstream*)handle)->is_open())
        {
            Close();
            return SetErrorIdAndReturn(EErrorId::FILE_IO_ACCESS);
        }

        return true;
    }

    bool TFileStream::IsGood() const noexcept
    {
        return IsOpen() ? (reinterpret_cast<std::fstream*>(handle))->good() : false;
    }

    bool TFileStream::IsBad() const noexcept
    {
        return IsOpen() ? (reinterpret_cast<std::fstream*>(handle))->bad() : false;
    }

    void TFileStream::Close() noexcept
    {
        if (handle)
        {
            delete (std::fstream*)handle;
            handle = nullptr;
        }
    }

    void TFileStream::Flush() noexcept
    {
        if (handle)
            (reinterpret_cast<std::fstream*>(handle))->flush();
    }

    uint32_t TFileStream::Read(void* a_buffer, uint32_t a_size) noexcept
    {
        if (!IsOpen() || !a_size) return 0;
        auto fstm = reinterpret_cast<std::fstream*>(handle);
        auto safe = static_cast<uint32_t>(fstm->tellg());
        fstm->read(reinterpret_cast<char*>(a_buffer), a_size);
        auto now = static_cast<uint32_t>(fstm->tellg());
        return now - safe;
    }

    uint32_t TFileStream::Write(const void* a_buffer, uint32_t a_size) noexcept
    {
        if (!IsOpen() || !a_size) return 0;
        auto fstm = reinterpret_cast<std::fstream*>(handle);
        auto safe = static_cast<uint32_t>(fstm->tellg());
        fstm->write(reinterpret_cast<const char*>(a_buffer), a_size);
        auto now = static_cast<uint32_t>(fstm->tellg());
        return now - safe;
    }

    uint32_t TFileStream::Seek(int32_t a_offset, EDirect a_direct) noexcept
    {
        TStreamGuard guard(locker);
        if (!IsOpen()) return 0;
        ResetError();
        auto fstm = reinterpret_cast<std::fstream*>(handle);
        fstm->seekg(a_offset, std::to_underlying(a_direct));
        return static_cast<uint32_t>(fstm->tellg());
    }

    // ===================== IFileStreamOp ===================== //

    bool IFileStreamOp::LoadFromFile(const char* a_filename) noexcept
    {
        TFileStream fstm(a_filename, TFileStream::EMode::OPEN_READ);
        return LoadFromStream(fstm);
    }

    bool IFileStreamOp::LoadFromFile(const std::string& a_filename) noexcept
    {
        return LoadFromFile(a_filename.c_str());
    }

    bool IFileStreamOp::LoadFromFile(const std::string_view& a_filename) noexcept
    {
        return LoadFromFile(a_filename.data());
    }

    bool IFileStreamOp::SaveToFile(const char* a_filename) const noexcept
    {
        TFileStream fstm(a_filename, TFileStream::EMode::CREATE);
        return SaveToStream(fstm);
    }

    bool IFileStreamOp::SaveToFile(const std::string& a_filename) const noexcept
    {
        return SaveToFile(a_filename.c_str());
    }

    bool IFileStreamOp::SaveToFile(const std::string_view& a_filename) const noexcept
    {
        return SaveToFile(a_filename.data());
    }

    // ====================== TPDBBinaryStream ==================== //

    TBinaryStream::TBinaryStream(const void* a_buffer, uint32_t a_size) noexcept
    {
        (void)WriteBuffer(a_buffer, a_size);
    }

    TBinaryStream::TBinaryStream(TBinaryStream&& a_rhs) noexcept
    {
        *this = a_rhs;
    }

    TBinaryStream::TBinaryStream(const TBinaryStream& a_rhs) noexcept
    {
        *this = a_rhs;
    }

    TBinaryStream& TBinaryStream::operator=(TBinaryStream&& a_rhs) noexcept
    {
        (void)CopyFrom(a_rhs);
        return *this;
    }

    TBinaryStream& TBinaryStream::operator=(const TBinaryStream& a_rhs) noexcept
    {
        (void)CopyFrom(const_cast<TBinaryStream&>(a_rhs));
        return *this;
    }

    bool TBinaryStream::Resize(uint32_t a_size) noexcept
    {
        ResetError();

        if (!a_size)
        {
            if (size)
                memory.reset();
            return true;
        }

        if (size)
        {
            auto sizeCopy = std::min(size, a_size);
            auto memCopy = std::make_unique<uint8_t[]>(sizeCopy);
            if (!memCopy)
                return SetErrorIdAndReturn(EErrorId::OUT_OF_MEMORY);

            if (!__TryCopyMemorySEH(memCopy.get(), memory.get(), sizeCopy))
                return SetErrorIdAndReturn(EErrorId::INTERNAL_EXCEPTION);

            memory.reset();
            memory = std::make_unique<uint8_t[]>(a_size);
            if (!memory)
                return SetErrorIdAndReturn(EErrorId::OUT_OF_MEMORY);

            if (__TryCopyMemorySEH(memory.get(), memCopy.get(), sizeCopy))
                return SetErrorIdAndReturn(EErrorId::INTERNAL_EXCEPTION);

            if (position > a_size)
                position = a_size;
            size = a_size;
        }
        else
        {
            memory = std::make_unique<uint8_t[]>(a_size);
            if (!memory)
                return SetErrorIdAndReturn(EErrorId::OUT_OF_MEMORY);

            position = 0;
            size = a_size;
        }

        return true;
    }

    uint32_t TBinaryStream::Read(void* a_buffer, uint32_t a_size) noexcept
    {
        uint32_t Ret = a_size;

        if (Ret > (size - position))
            Ret = size - position;

        if (!Ret)
            return Ret;

        if (!__TryCopyMemorySEH(a_buffer, memory.get() + position, Ret))
            return (uint32_t)SetErrorIdAndReturn(EErrorId::INTERNAL_EXCEPTION);

        position += Ret;
        return Ret;
    }

    uint32_t TBinaryStream::Write(const void* a_buffer, uint32_t a_size) noexcept
    {
        uint32_t Ret = a_size;

        if (Ret > (size - position))
        {
            if (!Resize(size + (Ret - (size - position))))
                return (uint32_t)SetErrorIdAndReturn(EErrorId::OUT_OF_MEMORY);
        }

        if (!__TryCopyMemorySEH(memory.get() + position, a_buffer, Ret))
            return (uint32_t)SetErrorIdAndReturn(EErrorId::INTERNAL_EXCEPTION);

        position += Ret;
        return Ret;
    }

    uint32_t TBinaryStream::Seek(int32_t a_offset, EDirect a_direct) noexcept
    {
        TStreamGuard guard(locker);
        ResetError();

        switch (a_direct)
        {
        case EDirect::BEGIN:
            position = (a_offset < 0) ? 0 : (static_cast<uint32_t>(a_offset) > size) ? size : a_offset;
            break;
        case EDirect::CURRENT:
            position += ((position + a_offset) > size) ? (size - (position + a_offset)) : a_offset;
            break;
        case EDirect::END:
            position = (a_offset >= 0) ? size : size + a_offset;
            break;
        }

        return position;
    }

    bool TBinaryStream::LoadFromStream(IStream& a_stream) noexcept
    {
        auto sizeCopy = a_stream.Size();
        if (!sizeCopy)
        {
            Clear();
            return true;
        }

        position = 0;
        if (!Resize(sizeCopy))
            return false;

        return CopyFrom(a_stream) == sizeCopy;
    }

    bool TBinaryStream::SaveToStream(IStream& a_stream) const noexcept
    {
        if (!size)
            return true;

        auto __this = const_cast<TBinaryStream*>(this);
        bool result = a_stream.CopyFrom(*__this) == size;
        return result;
    }

    // ====================== TTextFileStream ==================== //

    void TTextFileStream::WriteLineStr(const std::string& string) const noexcept
    {
        if (!IsOpen() || string.empty())
            return;

        fputs(string.c_str(), reinterpret_cast<FILE*>(handle));
        fputc('\n', reinterpret_cast<FILE*>(handle));
    }

    bool TTextFileStream::Open(const char* a_filename, EMode a_mode) noexcept
    {
        return Open(std::string(a_filename), a_mode);
    }

    bool TTextFileStream::Open(const std::string& a_filename, EMode a_mode) noexcept
    {
        if (IsOpen())
            Close();

        std::filesystem::path filename(TConvertUtil::ToDecode(a_filename));

        if (std::filesystem::is_directory(filename))
            return SetErrorIdAndReturn(EErrorId::INVALID_ARGUMENTS);

        if ((a_mode != EMode::CREATE) && (!std::filesystem::exists(filename)))
            return SetErrorIdAndReturn(EErrorId::FILE_NO_FOUND);

        if (a_mode == EMode::CREATE)
        {
            if (std::filesystem::exists(filename) && !std::filesystem::remove(filename))
                return SetErrorIdAndReturn(EErrorId::FILE_IO_ACCESS);
        }

        int right = _SH_DENYNO;
        std::wstring m;

        switch (a_mode)
        {
        case TFileStream::EMode::OPEN_READ:
            m = L"rb";
            right = _SH_DENYWR;
            break;
        case TFileStream::EMode::OPEN_READWRITE:
            m = L"rb+";
            right = _SH_DENYRW;
            break;
        default:
            m = L"w+";
            right = _SH_DENYRW;
            break;
        }

        handle = reinterpret_cast<void*>(_wfsopen(filename.wstring().c_str(), m.c_str(), right));
        if (!handle)
            return SetErrorIdAndReturn(a_mode == EMode::CREATE ? EErrorId::FILE_CANT_CREATE : EErrorId::FILE_CANT_OPEN);

        return true;
    }

    bool TTextFileStream::Open(const std::string_view& a_filename, EMode a_mode) noexcept
    {
        if (IsOpen())
            Close();

        std::filesystem::path filename(TConvertUtil::ToDecode(a_filename));

        if (std::filesystem::is_directory(filename))
            return SetErrorIdAndReturn(EErrorId::INVALID_ARGUMENTS);

        if ((a_mode != EMode::CREATE) && (!std::filesystem::exists(filename)))
            return SetErrorIdAndReturn(EErrorId::FILE_NO_FOUND);

        if (a_mode == EMode::CREATE)
        {
            if (std::filesystem::exists(filename) && !std::filesystem::remove(filename))
                return SetErrorIdAndReturn(EErrorId::FILE_IO_ACCESS);
        }

        int right = _SH_DENYNO;
        std::wstring m;

        switch (a_mode)
        {
        case TFileStream::EMode::OPEN_READ:
            m = L"rb";
            right = _SH_DENYWR;
            break;
        case TFileStream::EMode::OPEN_READWRITE:
            m = L"rb+";
            right = _SH_DENYRW;
            break;
        default:
            m = L"w+";
            right = _SH_DENYRW;
            break;
        }

        handle = reinterpret_cast<void*>(_wfsopen(filename.wstring().c_str(), m.c_str(), right));
        if (!handle)
            return SetErrorIdAndReturn(a_mode == EMode::CREATE ? EErrorId::FILE_CANT_CREATE : EErrorId::FILE_CANT_OPEN);

        return true;
    }

    bool TTextFileStream::IsGood() const noexcept
    {
        return IsOpen() ? ferror(reinterpret_cast<FILE*>(handle)) : false;
    }

    bool TTextFileStream::IsBad() const noexcept
    {
        return IsOpen() ? !IsGood() : false;
    }

    void TTextFileStream::Close() noexcept
    {
        if (IsOpen())
        {
            fclose(reinterpret_cast<FILE*>(handle));
            handle = nullptr;
        }
    }

    void TTextFileStream::Flush() noexcept
    {
        if (IsOpen())
            fflush(reinterpret_cast<FILE*>(handle));
    }

    uint32_t TTextFileStream::Read(void* a_buffer, uint32_t a_size) noexcept
    {
        if (!IsOpen() || !a_size) return 0;
        auto fstm = reinterpret_cast<FILE*>(handle);
        return static_cast<uint32_t>(fread(a_buffer, 1, a_size, fstm));
    }

    uint32_t TTextFileStream::Write(const void* a_buffer, uint32_t a_size) noexcept
    {
        if (!IsOpen() || !a_size) return 0;
        auto fstm = reinterpret_cast<FILE*>(handle);
        return static_cast<uint32_t>(fwrite(a_buffer, 1, a_size, fstm));
    }

    uint32_t TTextFileStream::Seek(int32_t a_offset, EDirect a_direct) noexcept
    {
        TStreamGuard guard(locker);
        if (!IsOpen()) return 0;
        ResetError();
        auto fstm = reinterpret_cast<FILE*>(handle);
        fseek(fstm, a_offset, std::to_underlying(a_direct));
        return static_cast<uint32_t>(ftell(fstm));
    }

    bool TTextFileStream::ReadLine(char* string, std::uint32_t maxsize) const noexcept(true)
    {
        if (!IsOpen() || !maxsize)
            return false;

        fgets(string, (int)maxsize, reinterpret_cast<FILE*>(handle));
        return true;
    }

    void TTextFileStream::WriteString(const std::string_view& formatted_string, ...) const noexcept(true)
    {
        if (!IsOpen())
            return;

        va_list ap;
        va_start(ap, &formatted_string);
        auto len = _vscprintf(formatted_string.data(), (va_list)ap);
        if (len < 1) return;

        std::string string_done;
        string_done.resize((std::size_t)len);
        if (string_done.empty()) return;
        vsprintf(string_done.data(), formatted_string.data(), (va_list)ap);
        va_end(ap);

        fputs(string_done.c_str(), reinterpret_cast<FILE*>(handle));
    }

    void TTextFileStream::WriteString(const std::wstring_view& formatted_string, ...) const noexcept(true)
    {
        if (!IsOpen())
            return;

        va_list ap;
        va_start(ap, &formatted_string);
        auto len = _vscwprintf(formatted_string.data(), (va_list)ap);
        if (len < 1) return;

        std::wstring string_done;
        string_done.resize((std::size_t)len);
        if (string_done.empty()) return;
        _vswprintf(string_done.data(), formatted_string.data(), (va_list)ap);
        va_end(ap);

        fputs(TConvertUtil::ToEncode(string_done).c_str(), reinterpret_cast<FILE*>(handle));
    }

    void TTextFileStream::WriteString(const char* formatted_string, ...) const noexcept(true)
    {
        if (!IsOpen())
            return;

        va_list ap;
        va_start(ap, &formatted_string);
        auto len = _vscprintf(formatted_string, (va_list)ap);
        if (len < 1) return;

        std::string string_done;
        string_done.resize((std::size_t)len);
        if (string_done.empty()) return;
        vsprintf(string_done.data(), formatted_string, (va_list)ap);
        va_end(ap);

        fputs(string_done.c_str(), reinterpret_cast<FILE*>(handle));
    }

    void TTextFileStream::WriteString(const wchar_t* formatted_string, ...) const noexcept(true)
    {
        if (!IsOpen())
            return;

        va_list ap;
        va_start(ap, &formatted_string);
        auto len = _vscwprintf(formatted_string, (va_list)ap);
        if (len < 1) return;

        std::wstring string_done;
        string_done.resize((std::size_t)len);
        if (string_done.empty()) return;
        _vswprintf(string_done.data(), formatted_string, (va_list)ap);
        va_end(ap);

        fputs(TConvertUtil::ToEncode(string_done).c_str(), reinterpret_cast<FILE*>(handle));
    }

    void TTextFileStream::WriteLine(const std::string_view& formatted_string, ...) const noexcept(true)
    {
        if (!IsOpen())
            return;

        va_list ap;
        va_start(ap, &formatted_string);
        auto len = _vscprintf(formatted_string.data(), (va_list)ap);
        if (len < 1) return;

        std::string string_done;
        string_done.resize((std::size_t)len);
        if (string_done.empty()) return;
        vsprintf(string_done.data(), formatted_string.data(), (va_list)ap);
        va_end(ap);

        WriteLineStr(string_done);
    }

    void TTextFileStream::WriteLine(const std::wstring_view& formatted_string, ...) const noexcept(true)
    {
        if (!IsOpen())
            return;

        va_list ap;
        va_start(ap, &formatted_string);
        auto len = _vscwprintf(formatted_string.data(), (va_list)ap);
        if (len < 1) return;

        std::wstring string_done;
        string_done.resize((std::size_t)len);
        if (string_done.empty()) return;
        _vswprintf(string_done.data(), formatted_string.data(), (va_list)ap);
        va_end(ap);

        WriteLineStr(TConvertUtil::ToEncode(string_done));
    }

    void TTextFileStream::WriteLine(const char* formatted_string, ...) const noexcept(true)
    {
        if (!IsOpen())
            return;

        va_list ap{};
        va_start(ap, &formatted_string);
        auto len = _vscprintf(formatted_string, (va_list)ap);
        if (len < 1) return;

        std::string string_done;
        string_done.resize((std::size_t)len);
        if (string_done.empty()) return;
        vsprintf(string_done.data(), formatted_string, (va_list)ap);
        va_end(ap);

        WriteLineStr(string_done);
    }

    void TTextFileStream::WriteLine(const wchar_t* formatted_string, ...) const noexcept(true)
    {
        if (!IsOpen())
            return;

        va_list ap{};
        va_start(ap, &formatted_string);
        auto len = _vscwprintf(formatted_string, (va_list)ap);
        if (len < 1) return;

        std::wstring string_done;
        string_done.resize((std::size_t)len);
        if (string_done.empty()) return;
        _vswprintf(string_done.data(), formatted_string, (va_list)ap);
        va_end(ap);

        WriteLineStr(TConvertUtil::ToEncode(string_done));
    }

    TTextFileStream::TTextFileStream(const char* a_filename, EMode a_mode) noexcept
    {
        (void)Open(a_filename, a_mode);
    }

    TTextFileStream::TTextFileStream(const std::string& a_filename, EMode a_mode) noexcept
    {
        (void)Open(a_filename, a_mode);
    }

    TTextFileStream::TTextFileStream(const std::string_view& a_filename, EMode a_mode) noexcept
    {
        (void)Open(a_filename, a_mode);
    }
}