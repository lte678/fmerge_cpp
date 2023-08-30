#pragma once

#include <memory>
#include <functional>
#include <stdexcept>


namespace fmerge::protocol {

    typedef std::function<void(void*,size_t)> ReadFunc;
    typedef std::function<void(const void*,size_t)> WriteFunc;


    enum class MsgType : unsigned short {
        Unknown,
        Ignore,
        Version,
        Changes,
        FileTransfer,
        StartSync,
        ConflictResolutions,
    };


    class GenericMessage {
    public:
        virtual void serialize(WriteFunc write) const = 0;
        virtual unsigned long length() const = 0;
        virtual MsgType type() const = 0;
    };


    template<class T>
    class Message : public GenericMessage {
    public:
        // Construct using T::copy_constructor and creating a new unique_ptr
        Message(const T& _payload) : payload(_payload) {}
        // Construct by moving existing unique_ptr
        Message(std::unique_ptr<T> _payload) : payload(_payload) {}
    private:
        std::unique_ptr<T> payload;
        std::string serialized_payload;
        bool serialized{false};
    public:
        void serialize(WriteFunc write) const {
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

        unsigned long length() const {
            if(!serialized) {
                _serialize();
            }
            return serialized_payload.length();
        }
    private:
        void _serialize() {
            std::stringstream buffer{};
            payload->serialize(buffer);
            serialized_payload = buffer.str();
            serialized = true;
        }
    };


    class EmptyMessage : public GenericMessage {
    public:
        EmptyMessage() = default;
        void serialize(WriteFunc write) const {};
        unsigned long length() const { return 0; };
    };

}