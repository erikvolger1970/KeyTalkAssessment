using CertServer.BackEndBridge;
using CertServer.Client.Pages;
using CertServer.Components;
using CertServer.MockBridge;
using CertServer.Shared;
using MudBlazor.Services;

var builder = WebApplication.CreateBuilder(args);

builder.AddServiceDefaults();

builder.Services.AddMudServices();

// Add services to the container.
builder.Services.AddRazorComponents()
    .AddInteractiveServerComponents()
    .AddInteractiveWebAssemblyComponents();

// register services
builder.Services.AddKeyedScoped<IRestApi, RealApi>("useLive");
builder.Services.AddKeyedScoped<IRestApi, MockApi>("useMock");

// register factory for IRestApi that will resolve the correct implementation based on the ApiMode configuration value
builder.Services.AddScoped((serviceProvider) => serviceProvider.GetRequiredKeyedService<IRestApi>(
    serviceProvider.GetRequiredService<IConfiguration>().GetValue<string>("ApiMode")));

var app = builder.Build();

app.MapDefaultEndpoints();

// Configure the HTTP request pipeline.
if (app.Environment.IsDevelopment())
{
    app.UseWebAssemblyDebugging();
}
else
{
    app.UseExceptionHandler("/Error", createScopeForErrors: true);
    // The default HSTS value is 30 days. You may want to change this for production scenarios, see https://aka.ms/aspnetcore-hsts.
    app.UseHsts();
}

app.UseStatusCodePagesWithReExecute("/not-found", createScopeForStatusCodePages: true);
app.UseHttpsRedirection();

app.UseAntiforgery();

app.MapStaticAssets();
app.MapRazorComponents<App>()
    .AddInteractiveServerRenderMode()
    .AddInteractiveWebAssemblyRenderMode()
    .AddAdditionalAssemblies(typeof(CertServer.Client._Imports).Assembly);

app.Run();
