using CertServer.Shared;

namespace CertServer.BackEndBridge;

public class RealApi : IRestApi
{
    public (IEnumerable<User> Users, string? Error) GetUsers()
    {
        return ([], "Rest api not available");
    }
}
