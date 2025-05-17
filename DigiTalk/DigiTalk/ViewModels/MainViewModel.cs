using System;
using System.Collections.ObjectModel;
using System.Net.Sockets;
using System.Reactive;
using System.Text;
using System.Threading;
using Avalonia.Controls;
using ReactiveUI;
using System.Threading.Tasks;
using DigiTalk.Models;
using DigiTalk.ViewModels;
using DigiTalk.Views;
using System.Linq;

namespace DigiTalk.ViewModels
{
    public class MainViewModel : ViewModelBase
    {
        private readonly Window _mainWindow;
        private TcpClient _client;
        private NetworkStream _stream;
        private string _username;
        private bool _isRunning;
        private Thread _receiveThread;

        private string _statusMessage = "Disconnected";
        public string StatusMessage
        {
            get => _statusMessage;
            set => this.RaiseAndSetIfChanged(ref _statusMessage, value);
        }

        private string _messageText;
        public string MessageText
        {
            get => _messageText;
            set => this.RaiseAndSetIfChanged(ref _messageText, value);
        }

        private Contact _selectedContact;
        public Contact SelectedContact
        {
            get => _selectedContact;
            set
            {
                this.RaiseAndSetIfChanged(ref _selectedContact, value);
                Messages.Clear();
                if (value != null)
                {
                    LoadMessageHistory();
                }
            }
        }

        public ObservableCollection<Contact> Contacts { get; } = new();
        public ObservableCollection<Message> Messages { get; } = new();

        public ReactiveCommand<Unit, Unit> SendMessageCommand { get; }
        public ReactiveCommand<Unit, Unit> RefreshContactsCommand { get; }
        public ReactiveCommand<Window, Unit> LoginCommand { get; }

        public MainViewModel()
        {
            SendMessageCommand = ReactiveCommand.Create(SendMessage);
            RefreshContactsCommand = ReactiveCommand.Create(RefreshContacts);
            LoginCommand = ReactiveCommand.CreateFromTask<Window>(LoginAsync);
        }

        private async Task LoginAsync(Window window)
        {
            var loginDialog = new LoginDialog();
            var result = await loginDialog.ShowDialog<LoginDialogResult>(window);

            if (result?.Result == true)
            {
                ConnectToServer(result.Username, result.Password, result.IsRegister);
            }
            else
            {
                Environment.Exit(0);
            }
        }

        private void ConnectToServer(string username, string password, bool isRegister)
        {
            try
            {
                _username = username;
                _client = new TcpClient();
                _client.Connect("localhost", 8080);
                _stream = _client.GetStream();

                var authData = isRegister
                    ? $"REGISTER:{username}:{password}"
                    : $"{username}:{password}";
                SendMessageToStream(authData);

                var response = ReceiveMessageFromStream();
                if (isRegister)
                {
                    if (!response.StartsWith("OK"))
                    {
                        StatusMessage = $"Registration failed: {response}";
                        return;
                    }
                    StatusMessage = $"Registered successfully as {username}";
                }
                if (!response.StartsWith("OK"))
                {
                    StatusMessage = $"Connection failed: {response}";
                    return;
                }

                StatusMessage = $"Connected as {username}";
                _isRunning = true;

                _receiveThread = new Thread(ReceiveMessages);
                _receiveThread.Start();

                RefreshContacts();
            }
            catch (Exception ex)
            {
                StatusMessage = $"Connection error: {ex.Message}";
            }
        }

        private void SendMessage()
        {
            if (SelectedContact == null || string.IsNullOrWhiteSpace(MessageText))
                return;

            try
            {
                var timestamp = DateTimeOffset.UtcNow.ToUnixTimeSeconds();
                var fullMessage = $"MSG:{SelectedContact.Name}:{timestamp}:{MessageText}";
                SendMessageToStream(fullMessage);

                Messages.Add(new Message
                {
                    Sender = _username,
                    Content = MessageText,
                    Timestamp = DateTimeOffset.FromUnixTimeSeconds(timestamp).DateTime,
                    IsOwnMessage = true
                });

                MessageText = string.Empty;
            }
            catch (Exception ex)
            {
                StatusMessage = $"Send error: {ex.Message}";
            }
        }

        private void RefreshContacts()
        {
            SendMessageToStream("GET_CONTACTS");
        }

        private void SendMessageToStream(string message)
        {
            var data = Encoding.UTF8.GetBytes(message);
            _stream.Write(data, 0, data.Length);
        }

        private string ReceiveMessageFromStream()
        {
            var buffer = new byte[4096];
            var bytesRead = _stream.Read(buffer, 0, buffer.Length);
            return Encoding.UTF8.GetString(buffer, 0, bytesRead);
        }

        private void ReceiveMessages()
        {
            try
            {
                while (_isRunning)
                {
                    var message = ReceiveMessageFromStream();

                    Avalonia.Threading.Dispatcher.UIThread.Post(() =>
                    {
                        if (message.StartsWith("CONTACTS:"))
                        {
                            ProcessContactsList(message);
                        }
                        else if (message.StartsWith("HISTORY:"))
                        {
                            ProcessHistoryMessages(message);
                        }
                        else if (message.StartsWith("MSG:"))
                        {
                            ProcessIncomingMessage(message);
                        }
                    });
                }
            }
            catch (Exception ex)
            {
                Avalonia.Threading.Dispatcher.UIThread.Post(() =>
                {
                    StatusMessage = $"Receive error: {ex.Message}";
                    _isRunning = false;
                });
            }
        }

        private void ProcessContactsList(string message)
        {
            Contacts.Clear();

            var parts = message.Split('|');
            var users = parts[0].Substring(9).Split(',', StringSplitOptions.RemoveEmptyEntries);
            var groups = parts[1].Substring(8).Split(',', StringSplitOptions.RemoveEmptyEntries);

            foreach (var user in users)
                Contacts.Add(new Contact { Name = user, IsGroup = false });

            foreach (var group in groups)
                Contacts.Add(new Contact { Name = group, IsGroup = true });
        }

        private async void LoadMessageHistory()
        {
            if (SelectedContact == null) return;

            try
            {
                var request = $"GET_HISTORY:{SelectedContact.Name}";
                SendMessageToStream(request);
            }
            catch (Exception ex)
            {
                StatusMessage = $"History load error: {ex.Message}";
            }
        }

        private void ProcessHistoryMessages(string message)
        {
            var historyItems = message.Substring(8).Split('|');
            foreach (var item in historyItems)
            {
                var parts = item.Split(':');
                if (parts.Length >= 4)
                {
                    var timestamp = DateTimeOffset.FromUnixTimeSeconds(long.Parse(parts[2])).DateTime;

                    Messages.Add(new Message
                    {
                        Sender = parts[1],
                        Content = string.Join(":", parts.Skip(3)), // На случай, если в сообщении есть ':'
                        Timestamp = timestamp,
                        IsOwnMessage = parts[1] == _username
                    });
                }
            }
        }

        private void ProcessIncomingMessage(string message)
        {
            var parts = message.Substring(4).Split(':');
            if (parts.Length >= 3)
            {
                var timestamp = DateTimeOffset.FromUnixTimeSeconds(long.Parse(parts[1])).DateTime;

                Messages.Add(new Message
                {
                    Sender = parts[0],
                    Content = string.Join(":", parts.Skip(2)),
                    Timestamp = timestamp,
                    IsOwnMessage = false
                });
            }
        }

        public void Disconnect()
        {
            _isRunning = false;
            _receiveThread?.Join();
            _stream?.Close();
            _client?.Close();
        }
    }
}