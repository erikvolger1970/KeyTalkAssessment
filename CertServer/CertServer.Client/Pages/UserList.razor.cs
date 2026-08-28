using CertServer.Shared;
using MudBlazor;
using System.Text.Json;

namespace CertServer.Client.Pages;

public partial class UserList
{
    private List<User>? _userData;

    private List<string> _events = new();

    private User? _selectedUser;

    protected override async Task OnInitializedAsync()
    {
        // todo: read from real or mock rest api
        _userData =
        [
            new User(1, "Alice", Guid.NewGuid(), "Company A"),
            new User(2, "Bob", Guid.NewGuid(), "Company B"),
            new User(3, "Charlie", Guid.NewGuid(), "Company C", "Software Engineer"),
            new User(4, "Donald", Guid.NewGuid(), "Company C", "Product Owner", 5),
        ];
    }

    // events
    private void StartedEditingItem(User user)
    {
        _events.Insert(0, $"Event = StartedEditingItem, Data = {JsonSerializer.Serialize(user)}");
    }

    private void CanceledEditingItem(User user)
    {
        // update code for backing data here if needed
        _events.Insert(0, $"Event = CanceledEditingItem, Data = {JsonSerializer.Serialize(user)}");
    }

    private Task<DataGridEditFormAction> CommittedItemChanges(User user)
    {
        // update code for the backing data here
        if (user.Id == 0) // new item
        {
            bool cellMode = _userData!.Remove(user); // for cell mode
            if (cellMode)
                _userData!.Insert(0, user);
            else
                _userData!.Add(user);
        }

        _events.Insert(0, $"Event = CommittedItemChanges, Data = {JsonSerializer.Serialize(user)}");

        return Task.FromResult(DataGridEditFormAction.Close);
    }
}