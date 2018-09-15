﻿namespace WeatherStation.Tests.Functions
module WundergroundForwarder =
    open System
    open System.Diagnostics    
    open Microsoft.Azure.WebJobs.Host
    open Expecto    
    open WeatherStation.Functions.Model
    open WeatherStation.Model
    open WeatherStation.Tests.Functions.DataSetup
    open WeatherStation.Functions.WundergroundForwarder
    open WeatherStation

    let log = {
        new TraceWriter(TraceLevel.Verbose) with
            override this.Trace event =
                printfn "%A" event }

    type LogMessageCompare =
        | Exact of string
        | Ignore
        | Test of (string -> bool)
    let logExpectations expectedMessages =                     
        let messageIndex = ref 0
        {   
            new TraceWriter(TraceLevel.Verbose) with
                override this.Trace event =
                    printfn "%A" event
                    let index = !messageIndex
                    let level, messageCompare = index |> Array.get expectedMessages
                    Expect.equal level event.Level "Unexpected log level"
                    match messageCompare with
                    | Exact message -> Expect.equal message event.Message "Unexpected message"
                    | Test test -> Expect.isTrue (test event.Message) "Unexpected message"
                    | Ignore -> ()
                    messageIndex := index + 1 }

    [<Tests>]
    let errorHandlingTests =

        let processEventHubMessage log weatherStation readings =
            let save _ = async { return () }
            processEventHubMessage 
                log 
                (fun _ _ _ _ -> async { failwith "Should not have posted to wunderground" })
                (fun _ _ -> async { return weatherStation} )
                save
                save
                (fun _ _ -> async { return readings })
                (async { return (fun _ _ -> failwith "Did not expect settings retrieval")})

        testList "Error Handling" [
            testCaseAsync "Empty message" (processEventHubMessage log None [] "")
            
            testAsync "Missing particle device" {
                let message =
                    """
                    {
                        "data": "100:4.006250:3864|d10.800000:86.500000a1.700000:15",
                        "device_id": "1e0037000751363130333334",
                        "event": "Reading",
                        "published_at": "2018-06-04T23:35:04.892Z"
                    }
                    """

                let log = logExpectations [|
                    TraceLevel.Info, Exact message
                    TraceLevel.Info, Exact "Parsed particle reading for device 1e0037000751363130333334"
                    TraceLevel.Info, Ignore
                    TraceLevel.Info, Exact "Searching for device Particle 1e0037000751363130333334 in registry"
                    TraceLevel.Error, Exact "Device [1e0037000751363130333334] is not provisioned"|]
                
                do!
                    processEventHubMessage log None [] message
            }
        ]

    let weatherStation = {
        DeviceType = string DeviceType.Particle
        DeviceId = "1e0037000751363130333334"
        WundergroundStationId = "K1234"
        WundergroundPassword = "fuzzybunny"
        DirectionOffsetDegrees = None
        Latitude = 0.0
        Longitude = 0.0
        LastReading = None
    }

    type WundergroundParameters = {
        StationId : string
        Password : string
        Values : ReadingValues list
    }

    [<Tests>]
    let validTests = 
        testList "Regression Tests" [
            testAsync "Insert sample record" {
                let! weatherStationRepository = AzureStorage.weatherStationRepository connectionString
                do! weatherStationRepository.Save weatherStation

                let! weatherStationReloaded = weatherStationRepository.Get Particle weatherStation.DeviceId
                Expect.isSome weatherStationReloaded "No WeatherStation found"
                Expect.equal weatherStation weatherStationReloaded.Value "WeatherStations are not equal"
            }
            testAsync "Reading for basic device" {
                do! loadWeatherStations [weatherStation]
                do! clearReadings

                let readingTime = (new DateTime(2018,9,14))

                let message =
                    sprintf
                        """
                        {
                            "data": "100:4.006250:3864|b1.0:2.0:3.0d10.800000:86.500000a1.700000:15",
                            "device_id": "%s",
                            "event": "Reading",
                            "published_at": "%s"
                        }
                        """
                        weatherStation.DeviceId
                        (readingTime.ToString())

                let expectedReadings = [
                    BatteryChargeVoltage 4.006250M<volts>
                    PanelVoltage 16.984615384615384615384615385M<volts>
                    TemperatureCelciusBarometer 1.0M<celcius>
                    PressurePascal 2.0m<pascal>
                    HumidityPercentBarometer 3.0m<percent>
                    TemperatureCelciusHydrometer 10.800000M<celcius>
                    HumidityPercentHydrometer 86.500000M<percent>
                    SpeedMetersPerSecond 1.700000M<meters/seconds>
                    DirectionSixteenths 15<sixteenths>
                    ReadingTime readingTime]
                
                let wundergroundParameters = ref None
                do!
                    
                    processEventHubMessageWithAzureStorage (fun stationId password values _ -> async {
                        wundergroundParameters := Some {StationId = stationId; Password = password; Values = values |> Seq.toList}
                    }) log message
                
                let wundergroundParameters = !wundergroundParameters
                Expect.isSome wundergroundParameters "No call to wunderground"

                let wundergroundParameters = wundergroundParameters.Value
                Expect.equal wundergroundParameters.StationId weatherStation.WundergroundStationId "Unexpected StationId"
                Expect.equal wundergroundParameters.Password weatherStation.WundergroundPassword "Unexpected Password"                        
                Expect.equal wundergroundParameters.Values expectedReadings "Unexpected readings"

                let! readingsRepository = AzureStorage.readingsRepository connectionString
                let! readings = readingsRepository.GetAll()

                match readings with
                | [reading] ->
                    Expect.equal reading.SourceDevice weatherStation.DeviceId "Unexpected DeviceId"
                    Expect.isGreaterThan reading.ReadingTime readingTime "Unexpected ReadingTime"
                    Expect.equal reading.SpeedMetersPerSecond 1.70 "Unexpected SpeedMetersPerSecond"
                | _ -> failwith "Unexpected readings"

                do! clearWeatherStations
                do! clearReadings
            }]
