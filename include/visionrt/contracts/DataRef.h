#pragma once

#include <memory>
#include <utility>
#include <vector>

namespace visionrt {

using Owner = std::shared_ptr<void>;

template<class T>
class DataRef
{
public:
    explicit DataRef(T value, std::vector<Owner> keepAlive = {})
        : storage_(std::make_shared<Storage>(std::make_shared<T>(std::move(value)), std::move(keepAlive)))
    {
    }

    T &get() const noexcept
    {
        return *storage_->value;
    }

    Owner owner() const noexcept
    {
        return storage_;
    }

    DataRef retainedBy(Owner dependency) const
    {
        return DataRef(std::make_shared<Storage>(std::shared_ptr<T>(storage_, storage_->value.get()),
                                                 std::vector<Owner>{std::move(dependency)}));
    }

private:
    struct Storage
    {
        std::vector<Owner> keepAlive;
        std::shared_ptr<T> value;

        Storage(std::shared_ptr<T> payload, std::vector<Owner> owners)
            : keepAlive(std::move(owners))
            , value(std::move(payload))
        {
        }
    };

    explicit DataRef(std::shared_ptr<Storage> storage)
        : storage_(std::move(storage))
    {
    }

    std::shared_ptr<Storage> storage_;
};

}
