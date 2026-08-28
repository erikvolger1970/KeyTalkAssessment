namespace CertServer.Shared;

public record User(int Id, string Name, Guid Key, string CompanyName, string Description = "", int ANumber = 0);
