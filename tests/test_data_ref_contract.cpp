#ifdef NDEBUG
#    undef NDEBUG
#endif

#include <visionrt/contracts/DataRef.h>
#include <opencv2/core.hpp>

#include <array>
#include <cassert>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

int main()
try
{
    std::size_t                                            releases = 0;
    std::weak_ptr<void>                                    bufferLifetime;
    std::optional<visionrt::DataRef<std::vector<cv::Mat>>> retained;
    {
        auto pixels    = std::shared_ptr<std::array<std::uint8_t, 6>>(new std::array<std::uint8_t, 6>{1, 2, 3, 4, 5, 6},
                                                                      [&releases](auto *storage)
                                                                      {
                                                                       ++releases;
                                                                       delete storage;
                                                                   });
        bufferLifetime = pixels;
        cv::Mat                                       image(2, 3, CV_8UC1, pixels->data());
        const visionrt::DataRef<std::vector<cv::Mat>> images({image}, {pixels});
        auto                                          alias = images;
        assert(&alias.get() == &images.get());
        assert(alias.get()[0].data == pixels->data());
        alias.get()[0].at<std::uint8_t>(0, 1) = 91;
        assert(images.get()[0].at<std::uint8_t>(0, 1) == 91);
        assert((*pixels)[1] == 91);
        alias.get().emplace_back(1, 1, CV_8UC1, cv::Scalar(17));
        assert(images.get().size() == 2);
        retained = alias;
    }
    assert(releases == 0 && !bufferLifetime.expired());
    assert(retained->get()[0].at<std::uint8_t>(0, 1) == 91);
    assert(retained->get()[1].at<std::uint8_t>(0, 0) == 17);
    const visionrt::DataRef<std::vector<cv::Mat>> independent(std::vector<cv::Mat>{retained->get()[0].clone()});
    retained->get()[0].at<std::uint8_t>(0, 1) = 44;
    assert(independent.get()[0].at<std::uint8_t>(0, 1) == 91);
    assert(independent.get()[0].data != retained->get()[0].data);
    retained.reset();
    assert(releases == 1 && bufferLifetime.expired());

    std::vector<std::string>                               destroyed;
    bool                                                   dependencyAlive = true;
    std::optional<visionrt::DataRef<std::vector<cv::Mat>>> dependent;
    {
        auto pixels = std::shared_ptr<std::array<std::uint8_t, 6>>(new std::array<std::uint8_t, 6>{1, 2, 3, 4, 5, 6},
                                                                   [&destroyed, &dependencyAlive](auto *storage)
                                                                   {
                                                                       assert(dependencyAlive);
                                                                       destroyed.emplace_back("pixels begin");
                                                                       delete storage;
                                                                       assert(dependencyAlive);
                                                                       destroyed.emplace_back("pixels end");
                                                                   });
        auto dependency = std::shared_ptr<int>(new int(7),
                                               [&destroyed, &dependencyAlive](auto *storage)
                                               {
                                                   destroyed.emplace_back("dependency");
                                                   dependencyAlive = false;
                                                   delete storage;
                                               });
        const visionrt::DataRef<std::vector<cv::Mat>> source({cv::Mat(2, 3, CV_8UC1, pixels->data())}, {pixels});
        dependent = source.retainedBy(dependency);
        assert(&dependent->get() == &source.get());
    }
    auto opaqueOwner = dependent->owner();
    dependent.reset();
    assert(destroyed.empty() && dependencyAlive);
    std::thread([owner = std::move(opaqueOwner)]() mutable { owner.reset(); }).join();
    assert((destroyed == std::vector<std::string>{"pixels begin", "pixels end", "dependency"}));
    assert(!dependencyAlive);

    using Allocation = std::unique_ptr<int, std::function<void(int *)>>;
    for (const bool moveAssignment : {false, true})
    {
        destroyed.clear();
        dependencyAlive = true;
        std::optional<visionrt::DataRef<Allocation>> assigned;
        {
            auto                                dependency = std::shared_ptr<int>(new int(7),
                                                                                  [&destroyed, &dependencyAlive](auto *storage)
                                                                                  {
                                                       destroyed.emplace_back("dependency");
                                                       dependencyAlive = false;
                                                       delete storage;
                                                   });
            const visionrt::DataRef<Allocation> source(Allocation(new int(42),
                                                                  [&destroyed, &dependencyAlive](auto *storage)
                                                                  {
                                                                      assert(dependencyAlive);
                                                                      destroyed.emplace_back("payload");
                                                                      delete storage;
                                                                  }));
            assigned = source.retainedBy(dependency);
        }
        auto &self = *assigned;
        self       = self;
        self       = std::move(self);
        assert(*assigned->get() == 42 && destroyed.empty());
        visionrt::DataRef<Allocation> replacement(Allocation(new int(9), [](auto *storage) { delete storage; }));
        if (moveAssignment)
            *assigned = std::move(replacement);
        else
            *assigned = replacement;
        assert((destroyed == std::vector<std::string>{"payload", "dependency"}));
        assert(!dependencyAlive && *assigned->get() == 9);
    }
    return 0;
}
catch (const std::exception &error)
{
    std::cerr << error.what() << '\n';
    return 1;
}
