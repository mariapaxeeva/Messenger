using Avalonia;
using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Markup.Xaml;
using DigiTalk.Models;
using DigiTalk.ViewModels;
using System.Linq;

namespace DigiTalk.Views
{
    public partial class CreateGroupDialog : Window
    {
        public CreateGroupDialog()
        {
            InitializeComponent();
#if DEBUG
            this.AttachDevTools();
#endif
        }
        private void InitializeComponent()
        {
            AvaloniaXamlLoader.Load(this);
        }

        private void CreateButton_Click(object sender, RoutedEventArgs e)
        {
            if (DataContext is GroupDialogViewModel vm)
            {
                if (string.IsNullOrWhiteSpace(vm.Groupname))
                    Close(new GroupCreationResult { Result = false });

                var selectedMembers = (vm.Contacts
                    .Where(c => c.IsSelected)
                    .Select(c => c.Contact.Name))
                    .ToList();

                if (!selectedMembers.Any())
                    Close(new GroupCreationResult { Result = false });

                Close(new GroupCreationResult
                {
                    Result = true,
                    Groupname = vm.Groupname,
                    MemberUsernames = selectedMembers
                });
            }
            else
            {
                Close(new GroupCreationResult { Result = false });
            }
        }

        private void CancelButton_Click(object sender, RoutedEventArgs e)
        {
            Close(new GroupCreationResult { Result = false });
        }
    }
}