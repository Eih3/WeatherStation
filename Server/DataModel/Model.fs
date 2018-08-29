namespace WeatherStation

module Model =

    open System    
    open FSharp.Azure.Storage.Table

    type SystemSetting = {
        Group : string
        Key : string
        Value : string
    }
    EntityIdentiferReader.GetIdentifier <- fun setting -> {PartitionKey = setting.Group; RowKey = setting.Value}

    type DeviceType =
        | Particle
        | Hologram

    type WeatherStation = {
        DeviceType : DeviceType
        DeviceId : string
        WundergroundStationId : string
        WundergroundPassword : string
        DirectionOffsetDegrees : int option
        Latitude : double
        Longitude : double
        LastReading : DateTime
    }        

    EntityIdentiferReader.GetIdentifier <- fun weatherStation -> {PartitionKey = string weatherStation.DeviceType; RowKey = weatherStation.DeviceId}

    type Reading = {
        RefreshIntervalSeconds : int
        DeviceTime : DateTime
        ReadingTime : DateTime
        SupplyVoltage : double
        BatteryChargeVoltage : double
        PanelVoltage : double
        TemperatureCelciusHydrometer : double
        TemperatureCelciusBarometer : double
        HumidityPercentHydrometer : double
        HumidityPercentBarometer : double
        PressurePascal : double
        GustMetersPerSecond : double
        SpeedMetersPerSecond : double
        DirectionSixteenths : double
        SourceDevice : string
        RowKey : string
    }

    EntityIdentiferReader.GetIdentifier <- fun reading -> {PartitionKey = reading.RowKey; RowKey = reading.SourceDevice}