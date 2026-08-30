namespace CertServer.Shared;

public interface IRestApi
{
    (IEnumerable<User> Users, string? Error) GetUsers();
}
