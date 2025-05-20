using Avalonia;
using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Markup.Xaml;
using DigiTalk.Models;
using DigiTalk.ViewModels;

namespace DigiTalk.Views
{
    public partial class InviteMemberDialog : Window
    {
        public InviteMemberDialog()
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

        private void InviteButton_Click(object sender, RoutedEventArgs e)
        {
            if (DataContext is InviteMemberViewModel vm)
            {
                Close(new InviteMemberResult
                {
                    Result = true,
                    Username = vm.SelectedContact.Name
                });
            }
            else
            {
                Close(new InviteMemberResult { Result = false });
            }
        }

        private void CancelButton_Click(object sender, RoutedEventArgs e)
        {
            Close(new InviteMemberResult { Result = false });
        }
    }
}