namespace CertServer.Shared;

public record User(int Id, string Name, Guid Key, string CompanyName, DateTime ExpirationDate, string Description = "", int ANumber = 0);
