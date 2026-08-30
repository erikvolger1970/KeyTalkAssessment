using CertServer.Shared;
using Microsoft.AspNetCore.Components;
using MudBlazor;
using System.Text.Json;

namespace CertServer.Client.Pages;

public partial class UserList
{
    [Inject]
    public required IRestApi Api { get; set; }

    private IEnumerable<User>? _userData;

    private readonly List<string> _events = [];

    private User? _selectedUser;

    protected override async Task OnInitializedAsync()
    {
        var (Users, Error) = Api.GetUsers();

        // I prefer result pattern but this tuple also works without adding extra classes
        if (Error != null)
            _events.Insert(0, $"Error = {Error}");
        else
            _userData = Users;
    }
}