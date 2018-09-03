namespace WeatherStation

module AzureStorage =
    open System
    open System.Configuration

    open Microsoft.WindowsAzure.Storage    
    open Microsoft.WindowsAzure.Storage.Table

    open WeatherStation.Cache

    let connection =
        let connectionString = ConfigurationManager.ConnectionStrings.["AzureStorageConnection"].ConnectionString
        let storageAccount = CloudStorageAccount.Parse connectionString
        storageAccount.CreateCloudTableClient()

    let deviceTypes =
        let cases =
            typedefof<Model.DeviceType>
            |> FSharp.Reflection.FSharpType.GetUnionCases
        [for case in cases -> case.Name]

    let private repositoryCache = new Cache<string, obj>()

    let private getOrCreateRepository<'TRepository> key (builder : CloudTableClient -> Async<'TRepository>) =
        let cacheValueBuilder =
            async {
                let! repository = builder connection
                return box repository }
        async {
            let! repository = repositoryCache.GetOrCreate(key, cacheValueBuilder)
            return unbox<'TRepository> repository
        }

    let weatherStationRepository = getOrCreateRepository "WeatherStations" Repository.createWeatherStationsRepository        
    let settingsRepository = getOrCreateRepository "SystemSettings" Repository.createSystemSettingRepository
    let readingsRepository = getOrCreateRepository "Readings" Repository.createReadingRepository