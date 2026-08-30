using CertServer.Shared;

namespace CertServer.MockBridge;

public class MockApi : IRestApi
{
    public (IEnumerable<User> Users, string? Error) GetUsers()
    {
        DateTime baseExpirationDate = DateTime.UtcNow.AddYears(1);
        List<User> users = [
            new User(1, "Bob", Guid.NewGuid(), "Company A", baseExpirationDate.AddYears(1)),
            new User(2, "Charlie", Guid.NewGuid(), "Company B", baseExpirationDate.AddMonths(1)),
            new User(3, "Donald", Guid.NewGuid(), "Company C", baseExpirationDate.AddDays(111), "Software Engineer"),
            new User(4, "Alice", Guid.NewGuid(), "Company C", baseExpirationDate.AddYears(-2), "Product Owner", 5),
        ];

        return (users, null);
    }
}
