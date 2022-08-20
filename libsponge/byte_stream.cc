#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity)
    : _capacity(capacity), _buffer(), end_write(false), written_bytes(0), read_bytes(0) {}

size_t ByteStream::write(const string &data) {
    size_t len = data.size();
    len = min(len, _capacity - _buffer.size());
    _buffer.append(BufferList(std::move(string().assign(data.begin(),data.begin()+len))));
    written_bytes += len;
    return len;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t CanPeek = min(len, _buffer.size());
    string s = _buffer.concatenate();
    return string().assign(s.begin(),s.begin()+CanPeek);
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    if (len > _buffer.size()) {
        set_error();
        return;
    }
    _buffer.remove_prefix(len);
    read_bytes += len;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    if (len > _buffer.size()) {
        set_error();
        return "";
    }
    const string out = peek_output(len);
    pop_output(len);
    return out;
}

void ByteStream::end_input() { end_write = true; }

bool ByteStream::input_ended() const { return end_write; }

size_t ByteStream::buffer_size() const { return _buffer.size(); }

bool ByteStream::buffer_empty() const { return _buffer.size() == 0; }

bool ByteStream::eof() const { return buffer_empty() && input_ended(); }

size_t ByteStream::bytes_written() const { return written_bytes; }

size_t ByteStream::bytes_read() const { return read_bytes; }

size_t ByteStream::remaining_capacity() const { return _capacity - _buffer.size(); }
