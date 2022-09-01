//  Copyright 2022 Lance Authors
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.

#include "lance/io/project.h"

#include <arrow/compute/exec/expression.h>
#include <arrow/dataset/dataset.h>
#include <arrow/table.h>
#include <arrow/type.h>

#include <catch2/catch_test_macros.hpp>
#include <string>
#include <vector>

#include "lance/arrow/scanner.h"
#include "lance/arrow/stl.h"
#include "lance/format/schema.h"
#include "lance/io/filter.h"
#include "lance/io/limit.h"
#include "lance/io/reader.h"
#include "lance/testing/io.h"

TEST_CASE("Project schema") {
  auto schema =
      ::arrow::schema({arrow::field("k", arrow::int16()), arrow::field("v", arrow::int32())});
  auto arr = arrow::StructArray::Make(::arrow::ArrayVector({
                                   lance::arrow::ToArray<int16_t>({1, 2, 3, 4}).ValueOrDie(),
                                   lance::arrow::ToArray<int32_t>({10, 20, 30, 40}).ValueOrDie(),
                               }),
                               std::vector<std::string>({"k", "v"}))
          .ValueOrDie();
  auto tbl =
      arrow::Table::FromRecordBatches({arrow::RecordBatch::FromStructArray(arr).ValueOrDie()})
          .ValueOrDie();
  auto dataset = std::make_shared<arrow::dataset::InMemoryDataset>(tbl);

  auto scan_builder = lance::arrow::ScannerBuilder(dataset);
  CHECK(scan_builder.Project({"v"}).ok());
  CHECK(scan_builder
            .Filter(
                arrow::compute::equal(arrow::compute::field_ref("v"), arrow::compute::literal(20)))
            .ok());
  auto scanner = scan_builder.Finish().ValueOrDie();

  auto lance_schema = lance::format::Schema(schema);
  auto project = lance::io::Project::Make(lance_schema, scanner->options()).ValueOrDie();

  auto reader = lance::testing::MakeReader(tbl).ValueOrDie();
  auto batch = project->Execute(reader, 0).ValueOrDie();
  INFO("Array v = " << batch->GetColumnByName("v")->ToString());
  CHECK(batch->GetColumnByName("v")->Equals(lance::arrow::ToArray({20}).ValueOrDie()));
}