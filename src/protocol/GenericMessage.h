#pragma once

#include <memory>
#include <functional>
#include <stdexcept>
#include <sstream>


namespace fmerge::protocol {

    typedef std::function<void(void*,size_t)> ReadFunc;
    typedef std::function<void(const void*,size_t)> WriteFunc;


    enum class MsgType : unsigned short {
        Unknown,
        Ignore,
        Version,
        Changes,
        FileTransfer,
        FileRequest,
        StartSync,
        ConflictResolutions,
    };


    class GenericMessage {
    public:
        virtual void serialize(WriteFunc write) = 0;
        virtual unsigned long length() = 0;
        virtual MsgType type() const = 0;
    };


    template<class T>
    class Message : public GenericMessage {
    public:
        Message() = delete;
        // Construct using T::copy_constructor and creating a new unique_ptr
        // Message(const T& _payload) : payload(std::make_unique(_payload)) {}
        // Construct by moving existing unique_ptr
        Message(std::unique_ptr<T> _payload) : payload(_payload) {}
    protected:
        std::unique_ptr<T> payload;
    private:
        std::string serialized_payload;
        bool serialized{false};
        MsgType _type{MsgType::Unknown};
    public:
        const T& get_payload() { return payload.get(); }

        void serialize(WriteFunc write) {
            if(!payload) {
                throw std::invalid_argument("attempted to serialize null-payload");
            }
            if(!serialized) { 
                _serialize();
            }
            write(serialized_payload.data(), length());
        }

        static std::shared_ptr<GenericMessage> deserialize(ReadFunc read, unsigned long length) {
            return std::make_shared<Message<T>>(T::deserialize(read, length));
        }

        unsigned long length() {
            if(!serialized) {
                _serialize();
            }
            return serialized_payload.length();
        }

        MsgType type() const override {
            return _type;
        };
    private:
        void _serialize() {
            std::stringstream buffer{};
            WriteFunc write_func = [&buffer](auto write_buf, auto write_len) {
                buffer.write(reinterpret_cast<const char*>(write_buf), write_len);
            };
            payload->serialize(write_func);
            serialized_payload = buffer.str();
            serialized = true;
        }
    };


    class EmptyMessage : public GenericMessage {
    public:
        EmptyMessage() = default;
        void serialize(WriteFunc) override {};
        unsigned long length() override { return 0; };
    };

}