using CertServer.Shared;

namespace CertServer.MockBridge;

public class MockApi : IRestApi
{
    public (IEnumerable<User> Users, string? Error) GetUsers()
    {
        List<User> users = [
            new User(1, "Alice", Guid.NewGuid(), "Company A"),
            new User(2, "Bob", Guid.NewGuid(), "Company B"),
            new User(3, "Charlie", Guid.NewGuid(), "Company C", "Software Engineer"),
            new User(4, "Donald", Guid.NewGuid(), "Company C", "Product Owner", 5),
        ];

        return (users, null);
    }
}
