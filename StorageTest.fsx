#load "Database.fsx"

open Database
open System
open System.IO
open Microsoft.Azure // Namespace for CloudConfigurationManager
open Microsoft.WindowsAzure.Storage // Namespace for CloudStorageAccount
open Microsoft.WindowsAzure.Storage.Table // Namespace for Table storage types

[<Literal>]
let WeatherStationStorageConnection = "DefaultEndpointsProtocol=https;AccountName=weatherstationsstorage;AccountKey=GQTMcwjw39JA7cY3wrdaRHdt6ApfafgiXrozLE27J5WpDiTEFs7yPlCtolWR6dEq3bfw4gccvwaeglzYOcpWmA==;BlobEndpoint=https://weatherstationsstorage.blob.core.windows.net/;QueueEndpoint=https://weatherstationsstorage.queue.core.windows.net/;TableEndpoint=https://weatherstationsstorage.table.core.windows.net/;FileEndpoint=https://weatherstationsstorage.file.core.windows.net/;"
let storageAccount = CloudStorageAccount.Parse(WeatherStationStorageConnection)
let tableClient = storageAccount.CreateCloudTableClient()

let reading = 
    Reading(
        PartitionKey = "TEST",
        RowKey = "2",
        BatteryVoltage = nullable (Some 1),
        RefreshIntervalSeconds = 1,
        DeviceTime = DateTime.MaxValue,
        ReadingTime = DateTime.MaxValue,
        SupplyVoltage = nullable (Some 1),
        ChargeVoltage = nullable (Some 1),
        TemperatureCelciusHydrometer = nullable (Some (double 1.1m)),
        TemperatureCelciusBarometer = nullable (Some (double 1.1m)),
        HumidityPercent = nullable (Some (double 1.1m)),
        PressurePascal = nullable (Some (double 1.1m)),
        SpeedMetersPerSecond = nullable (Some (double 1.1m)),
        DirectionSixteenths = nullable (Some (double 1.1m)),
        SourceDevice = "source")
let table = tableClient.GetTableReference("Readings")
table.CreateIfNotExists()

let insert = TableOperation.Insert(reading)

let result = table.Execute(insert)

printfn "%A" result
(*
type Foo() = 
    inherit TableEntity()
    member val Value = Option<int>.None with get, set

let foos = [
    Foo(RowKey = "1", PartitionKey = "1", Value = None)
    Foo(RowKey = "2", PartitionKey = "1", Value = Some(1))]

let testTable = tableClient.GetTableReference("Test")
testTable.DeleteIfExists()
testTable.Create()

for foo in foos do
    let insertResult = testTable.Execute(TableOperation.Insert(foo))
    printfn "%A" insertResult.Result

let query = testTable.CreateQuery<Foo>()
let results = query.Execute()

for foo in results do
    printfn "%s %s %A" foo.RowKey foo.PartitionKey foo.Value
    *)