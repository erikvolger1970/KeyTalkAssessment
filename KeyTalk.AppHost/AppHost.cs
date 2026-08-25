var builder = DistributedApplication.CreateBuilder(args);

builder.AddProject<Projects.CertServer>("certserver")
    .WithExternalHttpEndpoints()
    .WithEndpointsInEnvironment(x => x.UriScheme == "https");

builder.Build().Run();
