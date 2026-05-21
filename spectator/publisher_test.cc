#include "publisher.h"
#include "registry.h"

#include <gtest/gtest.h>

#include <memory>
#include <string_view>
#include <utility>

namespace
{

using spectator::GetConfiguration;
using spectator::Id;
using spectator::Measurement;
using spectator::Registry;
using spectator::Tags;

class TestPublisher : public spectator::Publisher<Registry>
{
   public:
	explicit TestPublisher(Registry* registry) : Publisher(registry) {}

	auto CommonTagCount(const Measurement& measurement) const -> size_t
	{
		return common_tags_size_for_measurement(measurement);
	}

	auto OmitsCommonTag(const Measurement& measurement, std::string_view key) const -> bool
	{
		return should_omit_common_tag(measurement, spectator::intern_str(key));
	}
};

auto new_registry() -> std::unique_ptr<Registry>
{
	auto config = GetConfiguration();
	config->common_tags = {{"nf.app", "hadoop"}, {"nf.node", "i-123"}, {"nf.region", "us-east-1"}};
	return std::make_unique<Registry>(std::move(config), spectatord::Logger());
}

TEST(PublisherMetricPolicy, KeepsNodeCommonTagForSystemMetrics)
{
	auto registry = new_registry();
	TestPublisher publisher{registry.get()};
	Id id{"node.cpu.usage", Tags{{"nf.process", "node-exporter"}}};
	Measurement measurement{id, 1.0};

	EXPECT_FALSE(publisher.OmitsCommonTag(measurement, "nf.node"));
	EXPECT_EQ(publisher.CommonTagCount(measurement), 3);
}

TEST(PublisherMetricPolicy, OmitsNodeCommonTagWhenNfProcessIsSparkExecutor)
{
	auto registry = new_registry();
	TestPublisher publisher{registry.get()};
	Id id{"jvm.cpu.usage", Tags{{"nf.process", "spark-executor"}}};
	Measurement measurement{id, 1.0};

	EXPECT_TRUE(publisher.OmitsCommonTag(measurement, "nf.node"));
	EXPECT_FALSE(publisher.OmitsCommonTag(measurement, "nf.app"));
	EXPECT_EQ(publisher.CommonTagCount(measurement), 2);
}

TEST(PublisherMetricPolicy, KeepsNodeCommonTagForExecutorIdWithoutSparkExecutorProcess)
{
	auto registry = new_registry();
	TestPublisher publisher{registry.get()};
	Id id{"jvm.cpu.usage", Tags{{"executorId", "3"}}};
	Measurement measurement{id, 1.0};

	EXPECT_FALSE(publisher.OmitsCommonTag(measurement, "nf.node"));
	EXPECT_EQ(publisher.CommonTagCount(measurement), 3);
}

TEST(PublisherMetricPolicy, KeepsNodeCommonTagForSparkExecutorNamedMetricsWithoutSparkExecutorProcess)
{
	auto registry = new_registry();
	TestPublisher publisher{registry.get()};
	Id id{"spark.executor.runTime", Tags{}};
	Measurement measurement{id, 1.0};

	EXPECT_FALSE(publisher.OmitsCommonTag(measurement, "nf.node"));
	EXPECT_EQ(publisher.CommonTagCount(measurement), 3);
}

TEST(PublisherMetricPolicy, AppliesConfigurablePolicyRule)
{
	auto config = GetConfiguration();
	config->common_tags = {{"nf.app", "hadoop"}, {"nf.node", "i-123"}, {"nf.region", "us-east-1"}};
	config->policies = {{{{"source", "custom"}}, {"nf.region", "nf.node"}}};
	Registry registry{std::move(config), spectatord::Logger()};
	TestPublisher publisher{&registry};
	Id id{"custom.metric", Tags{{"source", "custom"}}};
	Measurement measurement{id, 1.0};

	EXPECT_TRUE(publisher.OmitsCommonTag(measurement, "nf.node"));
	EXPECT_TRUE(publisher.OmitsCommonTag(measurement, "nf.region"));
	EXPECT_FALSE(publisher.OmitsCommonTag(measurement, "nf.app"));
	EXPECT_EQ(publisher.CommonTagCount(measurement), 1);
}

}  // namespace
